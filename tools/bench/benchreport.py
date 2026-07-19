#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: Copyright The C47 Authors
#
# Run the C47 benchmark suite in the optimized headless simulator and report
# the results, optionally predicting DM42n hardware times from a calibration
# file. See tools/bench/README.md for the full workflow.
#
# Usage:
#   python3 tools/bench/benchreport.py                 run and report
#   python3 tools/bench/benchreport.py --reps 3        fewer repetitions
#   python3 tools/bench/benchreport.py --calibrate "BMGTO=245,BMREG=180,..."
#       pair the given hardware tick readings (R80..R87 after XEQ BENCH on
#       the calculator) with a fresh local run and rewrite the calibration
#       file. Ticks are 0.1 s units, exactly as the programs store them.

import argparse
import csv
import json
import os
import statistics
import subprocess
import sys
from datetime import date
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BENCH_BIN = REPO / "t47bench"
SCRIPT = REPO / "res" / "SCRIPTS" / "bench.t47"
CALIBRATION = Path(__file__).resolve().parent / "calibration-dm42n.json"
CSV_OUT = REPO / "bench-results.csv"

BENCHMARKS = ["BMGTO", "BMREG", "BMVAR", "BMRSV", "BMARITH", "BMTRIG", "BMNQN", "BMDISP"]
# Iterations fixed in tools/bench/genbench.py; kept here only to report it/s.
ITERATIONS = {
    "BMGTO": 20000,
    "BMREG": 10000,
    "BMVAR": 5000,
    "BMRSV": 10000,
    "BMARITH": 3000,
    "BMTRIG": 500,
    "BMNQN": 2,
    "BMDISP": 2000,
}
# The simulator's display path (GTK) does not model the hardware's DMA LCD
# writes, so no hardware prediction is offered for BMDISP.
HW_ONLY = {"BMDISP"}
SPREAD_WARN = 0.10


def run_suite(reps):
    if not BENCH_BIN.exists():
        sys.exit(f"{BENCH_BIN} not found - run 'make benchbin' first")
    env = dict(os.environ, BENCH_REPS=str(reps))
    cmd = [str(BENCH_BIN), "--headless", "--snapskiprefresh", "--reset", "--script", str(SCRIPT)]
    proc = subprocess.run(cmd, cwd=REPO, env=env, capture_output=True, text=True)
    lines = [l for l in proc.stdout.splitlines() if l.startswith("BENCH|")]
    if not lines or lines[-1] != "BENCH|END":
        sys.stderr.write(proc.stdout[-2000:] + proc.stderr[-2000:])
        sys.exit("benchmark run did not complete (no BENCH|END)")
    canary = None
    results = {}
    for line in lines[:-1]:
        parts = line.split("|")
        if parts[1] == "CANARY":
            canary = int(parts[2])
            continue
        name, _rep, us, ticks = parts[1], parts[2], int(parts[3]), int(parts[4])
        results.setdefault(name, []).append((us, ticks))
    if canary != ITERATIONS["BMGTO"]:
        sys.exit(f"BMGTO loop canary mismatch: R96={canary}, expected {ITERATIONS['BMGTO']}")
    return results


def summarize(results):
    rows = {}
    for name in BENCHMARKS:
        pairs = results.get(name)
        if not pairs:
            sys.exit(f"no results for {name}")
        us = [p[0] for p in pairs]
        med_s = statistics.median(us) / 1e6
        spread = (max(us) - min(us)) / statistics.median(us) if len(us) > 1 else 0.0
        ticks = pairs[-1][1]
        # The program's own TICKS bracket must agree with wall clock to
        # within its 0.1 s quantization (plus one tick of slack).
        if abs(ticks / 10.0 - med_s) > 0.25 and ticks / 10.0 > med_s:
            sys.exit(f"{name}: on-calc ticks {ticks} disagree with wall clock {med_s:.3f}s")
        rows[name] = {"median_s": med_s, "spread": spread, "ticks": ticks}
    return rows


def load_calibration():
    if not CALIBRATION.exists():
        return None
    data = json.loads(CALIBRATION.read_text())
    return data if data.get("benchmarks") else None


def report(rows, calib):
    factors = {}
    if calib:
        for name, entry in calib["benchmarks"].items():
            if entry.get("hw_ticks") and entry.get("local_s"):
                factors[name] = (entry["hw_ticks"] / 10.0) / entry["local_s"]

    header = f"{'benchmark':<9} {'iters':>7} {'local s':>9} {'spread':>7} {'it/s':>12} {'DM42n s':>9}"
    print(header)
    print("-" * len(header))
    csv_rows = []
    for name in BENCHMARKS:
        r = rows[name]
        its = ITERATIONS[name] / r["median_s"]
        if name in HW_ONLY:
            pred = "hw-only"
        elif name in factors:
            pred = f"{r['median_s'] * factors[name]:9.1f}"
        else:
            pred = "uncal."
        warn = " !" if r["spread"] > SPREAD_WARN else ""
        print(f"{name:<9} {ITERATIONS[name]:>7} {r['median_s']:>9.4f} {r['spread']:>6.1%} {its:>12.0f} {pred:>9}{warn}")
        csv_rows.append({
            "benchmark": name,
            "iterations": ITERATIONS[name],
            "local_median_s": f"{r['median_s']:.6f}",
            "spread": f"{r['spread']:.4f}",
            "iterations_per_s": f"{its:.1f}",
            "predicted_dm42n_s": pred.strip(),
        })
    if calib:
        print(f"\ncalibration: {calib.get('device')} | {calib.get('power')} | "
              f"firmware {calib.get('firmware')} | {calib.get('date')}")
        spread_f = sorted(factors.values())
        if len(spread_f) > 1:
            print(f"hardware-tax factors span {spread_f[0]:.0f}x..{spread_f[-1]:.0f}x local time "
                  f"(spread is the per-workload hardware cost profile)")
    else:
        print("\nuncalibrated: no DM42n predictions. Run the suite on hardware "
              "(XEQ BENCH, read R80..R87) and pass --calibrate.")
    if any(rows[n]["spread"] > SPREAD_WARN for n in BENCHMARKS):
        print("!  spread above 10% - rerun on AC power with the machine idle")

    with CSV_OUT.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(csv_rows[0].keys()))
        writer.writeheader()
        writer.writerows(csv_rows)
    print(f"\nwrote {CSV_OUT}")


def calibrate(spec, rows, device, power, firmware):
    hw = {}
    for part in spec.split(","):
        name, _, ticks = part.partition("=")
        name = name.strip().upper()
        if name not in BENCHMARKS:
            sys.exit(f"--calibrate: unknown benchmark '{name}'")
        hw[name] = int(ticks)
    missing = [n for n in BENCHMARKS if n not in hw and n not in HW_ONLY]
    if missing:
        print(f"note: no hardware ticks given for {' '.join(missing)}; they stay uncalibrated")
    data = {
        "device": device,
        "power": power,
        "firmware": firmware,
        "date": date.today().isoformat(),
        "local_host": f"{os.uname().sysname} {os.uname().machine}, build.sim.t47.bench (-Os, LTO)",
        "benchmarks": {
            name: {"hw_ticks": hw[name], "local_s": round(rows[name]["median_s"], 6)}
            for name in hw
        },
    }
    CALIBRATION.write_text(json.dumps(data, indent=2) + "\n")
    print(f"wrote {CALIBRATION}")


def main():
    parser = argparse.ArgumentParser(description="Run the C47 benchmark suite and report")
    parser.add_argument("--reps", type=int, default=5, help="repetitions per benchmark (default 5)")
    parser.add_argument("--calibrate", metavar="SPEC",
                        help='hardware ticks per benchmark, e.g. "BMGTO=245,BMREG=180"')
    parser.add_argument("--device", default="DM42n (STM32U575)", help="calibration device label")
    parser.add_argument("--power", default="battery (80MHz)", help="calibration power state")
    parser.add_argument("--firmware", default="unspecified", help="calibration firmware version")
    args = parser.parse_args()

    rows = summarize(run_suite(args.reps))
    if args.calibrate:
        calibrate(args.calibrate, rows, args.device, args.power, args.firmware)
    report(rows, load_calibration())


if __name__ == "__main__":
    main()
