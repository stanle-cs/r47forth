# C47 benchmark suite

I built this to answer one question without guesswork: how fast is C47 on
the calculator in my hand? The test suite proves correctness, but nothing
measured speed. Performance work was running on stopwatches and intuition.
This apparatus replaces that with numbers you can reproduce.

The idea is simple. A set of small RPN programs, each isolating one cost
axis of the interpreter, runs identically in two places: on real hardware,
where each program times itself with TICKS, and in an optimized headless
simulator build, where a script times each run to the microsecond. Run the
suite once on your calculator to calibrate, and from then on a local run
predicts hardware seconds per benchmark. You can evaluate an optimization
on your PC in thirty seconds and know what it will do on the device.

## Quick start

```
make benchbin      # one time: optimized headless build, produces ./t47bench
make bench         # run the suite, print the report, write bench-results.csv
```

The report shows, per benchmark: the local median over five repetitions,
the spread (flagged above 10%), iterations per second, and predicted
hardware seconds if a calibration file is present. `--reps N` changes the
repetition count. The first execution of each benchmark is a discarded
warm-up.

Always use `t47bench`, never the default debug simulator. The debug build
measured 2.2x to 2.8x slower, and unevenly across benchmarks, which
distorts exactly the ratios this suite exists to measure. The bench build
uses -Os and LTO to mirror the DMCP hardware flags.

## The benchmarks

Every program follows the same shape: a TICKS bracket around a counted
DSZ/GTO loop, elapsed time in tenths of seconds left in X and stored in
R97. Iteration counts are fixed inside the programs and sized from real
DM42n measurements, so each benchmark runs long enough on hardware for
under 1% timing quantization while the whole suite stays short: measured
at about 3.5 minutes on USB power and 7.5 minutes on battery.

| program | axis |
|---|---|
| BMGTO | interpreter dispatch and local-label GTO scan, plus the per-step key poll only hardware pays |
| BMREG | numbered-register STO/RCL, the direct-indexed control case |
| BMVAR | named user-variable STO/RCL, the linear lookup of issue #555, with ten decoy variables |
| BMRSV | reserved-variable STO/RCL (FV), the gperf-hashed fast path |
| BMARITH | decNumber basics: + x / sqrt |
| BMTRIG | decNumber transcendentals: sin, ln, exp |
| BMNQN | macro benchmark: two runs of NQueens(8) via the stock NQueens program |
| BMDISP | display op cost (PIXEL); measured locally but never used to predict hardware, because the simulator draws through GTK and the hardware through DMA |

BENCHALL.p47 (label BENCH) is the driver: it runs all eight in order and
copies each result to R80 through R87. BMNQN calls the stock NQueens
program by its label NQ, so NQueens.p47 must be loaded too.

A derived diagnostic worth knowing: (BMVAR - BMREG) / (BMRSV - BMREG)
isolates the cost ratio of the linear user-variable scan against the
hashed reserved path. It measures about 4.9x in the simulator, 4.7x on
my DM42n on USB power and 4.2x on battery, which is the evidence that
the local proxy tracks hardware for lookup work.

## Status protocol

A hardware run takes minutes and any keypress halts a running program, so
the suite tells you what it is doing:

- Before each benchmark starts, its name is shown for one second.
- While a benchmark runs, the display counts R95 down from 10 to 1
  (BMNQN counts from 2, one per NQueens run). If that number keeps
  stepping, it is running. Leave it alone.
- The suite ends with a BEEP and the string DONE in X.

Failure forensics: the driver pre-fills R80 through R87 with the sentinel
999999 and keeps the index of the benchmark it is currently running in
R89 (1 through 8 in suite order, 9 when complete). After an interrupted
or failed run, any register still holding 999999 names a benchmark that
never finished, and R89 names the one that was interrupted. The countdown
refreshes sit inside the timed bracket by design; they cost about 1% on
hardware and the program structure is identical everywhere, so
comparisons stay honest.

## Running on the calculator

Copy the nine files from `res/PROGRAMS/bench/` plus `NQueens.p47` into
the calculator's PROGRAMS directory. To expose that directory as a USB
disk: shift, then the +/- key opens the MODE menu, then shift F2 is
ActUSB; confirm, copy the files over, eject. Then, on a DM42 style
keyboard with the default C47 layout:

1. Open the I/O menu: shift shift, then the minus key.
2. READP is the middle menu row, first position: shift, then F1. Pick a
   file with the arrow keys, ENTER loads it. Repeat for all ten files,
   then EXIT.
3. Run: press XEQ, then the PROG softkey, then the softkey under BENCH.
   Do not touch the keyboard until it beeps and shows DONE. Measured on
   my DM42n the suite takes about 3.5 minutes on USB power and about
   7.5 minutes on battery.
4. Read results: RCL 8 0 through RCL 8 7, in benchmark order (BMGTO,
   BMREG, BMVAR, BMRSV, BMARITH, BMTRIG, BMNQN, BMDISP). Each value is
   whole tenths of seconds.

Decide your power state before you start and keep it for the whole run.
USB power runs the DM42n at 160 MHz, battery at 80 MHz, and a mixed run
is useless for calibration.

Reloading changed programs: READP appends, and with duplicate labels the
first-loaded copy wins, so delete old copies before loading new ones.
Shift shift backspace opens the CLR menu, shift shift F6 opens DELETE,
F5 is DELP, then PROG and the label softkey. Repeat per program. Do not
press DELPall next to it, which deletes every program you have.

The suite writes registers R80 to R87, R89, R95 to R99 and R10 to R14,
and allocates named variables (VV, ten decoys AA through AJ, and it
overwrites the reserved variable FV). BMNQN runs NQueens, which uses
named variables of its own (nn, tt, xx, cc, yy). Run it on a state you do not mind touching, or SAVEST first
and LOADST after.

## Calibration

Calibration pairs one hardware run with one local run and stores a
per-benchmark scale factor under a named power profile. With the eight
hardware readings in hand:

```
python3 tools/bench/benchreport.py \
    --calibrate "BMGTO=212,BMREG=384,BMVAR=291,BMRSV=426,BMARITH=193,BMTRIG=147,BMNQN=246,BMDISP=116" \
    --profile usb-160mhz --power "USB (160MHz)" \
    --device "DM42n (STM32U575)" --firmware "C47 00.109.03.03b0"
```

This runs the suite locally, writes the profile into
`tools/bench/calibration-dm42n.json` without touching other profiles, and
every later `make bench` prints one predicted-seconds column per profile.
The math is one line per benchmark: factor = hardware seconds divided by
local seconds, prediction = local seconds times factor.

Calibrate USB and battery separately. They are not the same machine: the
clock halves on battery, but my unit measured battery-to-USB ratios from
1.9x (BMREG, BMDISP) up to 3.2x (BMTRIG), so the compute-bound paths lose
more than the clock ratio and no single scale factor could describe both
states.

The factors are per benchmark rather than one global number because the
costs only hardware pays (the per-step keyboard poll, LCD writes, flash
wait states) scale differently with each instruction mix. The spread of
the factors is itself a profile of that hardware tax. On my DM42n on USB
power it runs from 81x on BMTRIG, where decNumber compute dominates, to
369x on BMREG,
where fixed per-step interpreter overhead dominates. That spread is the
single most useful thing the suite has shown: for ordinary program loops,
what a hardware user feels is per-step overhead, not arithmetic.

Recalibrate after a firmware upgrade, after changing the local machine,
or when moving between power states. The calibration file records device,
power, firmware, date, and host, so a stale calibration is at least an
honest one.

## Regenerating the programs

The .p47 files are generated, never hand-edited:

```
python3 tools/bench/genbench.py           # regenerate res/PROGRAMS/bench/
python3 tools/bench/genbench.py --check   # verify checked-in files match
```

The generator parses item numbers out of `src/c47/items.h` at run time
and assembles the program byte streams directly, so the programs cannot
drift from the source of truth. Iteration counts live in one table in the
generator and are frozen: hardware and local runs stay comparable only as
long as N never changes, so changing any N invalidates every published
calibration. Do not resize casually.

## Limits, stated plainly

- Predictions are a calibrated model, not a cycle simulation. Expect
  fidelity within a few percent for interpreter-bound work, and worse
  wherever a change shifts the ratio of hardware-only costs.
- BMDISP is hardware-meaningful only. The local number exists to track
  simulator regressions, nothing more.
- Keyboard-to-display latency, how snappy the calculator feels under
  your fingers, is not measurable by this apparatus at all. That needs
  external instrumentation.
- Local numbers move with host load and thermals. Run plugged in, on an
  otherwise idle machine, and rerun anything whose spread exceeds 10%.
