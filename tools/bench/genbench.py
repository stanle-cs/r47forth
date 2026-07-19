#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# SPDX-FileCopyrightText: Copyright The C47 Authors
#
# Generate the benchmark suite programs in res/PROGRAMS/bench/.
#
# Each benchmark isolates one cost axis of the program interpreter and times
# itself with a TICKS bracket (the pattern NQueens.p47 already uses), so the
# same program is measurable on the calculator (elapsed ticks in X and in
# R97) and in the t47 headless simulator (microsecond wall clock around xeq,
# ticks via "reg 97"). The iteration count N of every benchmark is FIXED
# here: hardware and simulator runs stay comparable only as long as N never
# changes, so treat the N values as frozen once a calibration file has been
# published (see tools/bench/README.md).
#
# Harness register convention (documented in the README):
#   R99 loop counter   R98 start ticks   R97 elapsed ticks   R96 loop canary
#   R80..R87 per-benchmark results stored by the BENCH driver program
#   R10..R14 benchmark operands
#
# Item numbers are parsed from src/c47/items.h at generation time, and the
# program byte encoding follows src/c47/programming/nextStep.c
# (countOpBytes) and the examples in src/testSuite/testSuite.c
# (covWriteAndLoadPgm): items < 128 are one byte, items >= 128 are
# (n >> 8) | 0x80 followed by n & 0xff; a numbered-register parameter is a
# single byte 0-99; a named label/variable parameter is 253, length, bytes;
# a literal is ITM_LITERAL, type, length, ASCII (type 8 = long integer,
# type 9 = real34).
#
# Usage:
#   python3 tools/bench/genbench.py           regenerate res/PROGRAMS/bench/
#   python3 tools/bench/genbench.py --check   verify checked-in files match

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
ITEMS_H = REPO / "src" / "c47" / "items.h"
OUT_DIR = REPO / "res" / "PROGRAMS" / "bench"

STRING_LABEL_VARIABLE = 253
STRING_LONG_INTEGER = 8
STRING_REAL34 = 9

COUNTER_REG = 99
START_REG = 98
RESULT_REG = 97
CANARY_REG = 96
DRIVER_RESULT_BASE = 80


def load_items():
    items = {}
    for m in re.finditer(r"#define\s+(ITM_\w+)\s+(\d+)", ITEMS_H.read_text()):
        items[m.group(1)] = int(m.group(2))
    return items


ITEMS = load_items()


def op(name):
    n = ITEMS[name]
    return [n] if n < 128 else [(n >> 8) | 0x80, n & 0xff]


def named(opname, s):
    return op(opname) + [STRING_LABEL_VARIABLE, len(s)] + [ord(c) for c in s]


def reg(opname, r):
    return op(opname) + [r]


def lit_int(value):
    s = str(value)
    return op("ITM_LITERAL") + [STRING_LONG_INTEGER, len(s)] + [ord(c) for c in s]


def lit_real(s):
    return op("ITM_LITERAL") + [STRING_REAL34, len(s)] + [ord(c) for c in s]


def harness(label, n, setup, body):
    # LBL "label" / setup / N STO 99 / TICKS STO 98 / LBL 00 / body /
    # DSZ 99 GTO 00 / TICKS RCL 98 - STO 97 / RTN / END
    b = []
    b += named("ITM_LBL", label)
    b += setup
    b += lit_int(n) + reg("ITM_STO", COUNTER_REG)
    b += op("ITM_TICKS") + reg("ITM_STO", START_REG)
    b += reg("ITM_LBL", 0)
    b += body
    b += reg("ITM_DSZ", COUNTER_REG)
    b += reg("ITM_GTO", 0)
    b += op("ITM_TICKS")
    b += reg("ITM_RCL", START_REG)
    b += op("ITM_SUB")
    b += reg("ITM_STO", RESULT_REG)
    b += op("ITM_RTN")
    b += op("ITM_END")
    return b


def repeated(body, times):
    return body * times


# The decoy variables allocated before the measured variable make the
# linear scan in findNamedVariable() realistic: the measured name is
# deliberately the last one allocated, so every lookup walks the whole list.
BMVAR_DECOYS = ["AA", "AB", "AC", "AD", "AE", "AF", "AG", "AH", "AI", "AJ"]


def bmgto():
    # Dispatch + local-label GTO scan; the ISZ doubles as the loop canary
    # (R96 == N after a run proves the loop executed N times).
    setup = lit_int(0) + reg("ITM_STO", CANARY_REG)
    body = reg("ITM_ISZ", CANARY_REG)
    return harness("BMGTO", 200000, setup, body)


def bmreg():
    setup = lit_real("1.5") + reg("ITM_STO", 10)
    body = repeated(reg("ITM_RCL", 10) + reg("ITM_STO", 11), 5)
    return harness("BMREG", 20000, setup, body)


def bmvar():
    setup = lit_real("1.5")
    for name in BMVAR_DECOYS:
        setup += named("ITM_STO", name)
    setup += named("ITM_STO", "VV")
    body = repeated(named("ITM_RCL", "VV") + named("ITM_STO", "VV"), 5)
    return harness("BMVAR", 20000, setup, body)


def bmrsv():
    # FV is a reserved variable resolved through the gperf hash
    # (reservedRegisterLookup.h) - the fast path BMVAR's user variable
    # cannot take, and the variable upstream used to benchmark issue #555.
    setup = lit_real("1.5") + named("ITM_STO", "FV")
    body = repeated(named("ITM_RCL", "FV") + named("ITM_STO", "FV"), 5)
    return harness("BMRSV", 20000, setup, body)


def bmarith():
    # 1.5+2.5=4, 4*2.5=10, 10/2.5=4, sqrt(4)=2: value-stable every iteration.
    setup = lit_real("1.5") + reg("ITM_STO", 10) + lit_real("2.5") + reg("ITM_STO", 11)
    once = (
        reg("ITM_RCL", 10)
        + reg("ITM_RCL", 11)
        + op("ITM_ADD")
        + reg("ITM_RCL", 11)
        + op("ITM_MULT")
        + reg("ITM_RCL", 11)
        + op("ITM_DIV")
        + op("ITM_SQUAREROOTX")
    )
    return harness("BMARITH", 20000, setup, repeated(once, 2))


def bmtrig():
    # sin -> ln -> exp of a small positive value: defined in any angle mode.
    setup = lit_real("0.7") + reg("ITM_STO", 12)
    once = reg("ITM_RCL", 12) + op("ITM_sin") + op("ITM_LN") + op("ITM_EXP")
    return harness("BMTRIG", 4000, setup, repeated(once, 2))


def bmnqn():
    # Macro benchmark: real instruction mix. Calls the stock NQueens program
    # (global label NQ, argument 8 on the stack), so NQueens.p47 must be
    # loaded alongside this file; the bench runner and the README both do so.
    body = lit_int(8) + named("ITM_XEQ", "NQ")
    return harness("BMNQN", 5, [], body)


def bmdisp():
    # Display-path op cost. The simulator number is measured but not
    # hardware-predictive (GTK redraw vs DMA refresh); see the README.
    setup = lit_int(100) + reg("ITM_STO", 13) + lit_int(150) + reg("ITM_STO", 14)
    body = repeated(reg("ITM_RCL", 13) + reg("ITM_RCL", 14) + op("ITM_PIXEL"), 5)
    return harness("BMDISP", 2000, setup, body)


BENCHMARKS = ["BMGTO", "BMREG", "BMVAR", "BMRSV", "BMARITH", "BMTRIG", "BMNQN", "BMDISP"]
GENERATORS = {
    "BMGTO": bmgto,
    "BMREG": bmreg,
    "BMVAR": bmvar,
    "BMRSV": bmrsv,
    "BMARITH": bmarith,
    "BMTRIG": bmtrig,
    "BMNQN": bmnqn,
    "BMDISP": bmdisp,
}


def benchall():
    # One-shot driver for a hardware run: XEQ each benchmark and copy its
    # elapsed ticks from R97 to R80.. so all results survive the whole run.
    b = named("ITM_LBL", "BENCH")
    for i, bm in enumerate(BENCHMARKS):
        b += named("ITM_XEQ", bm)
        b += reg("ITM_RCL", RESULT_REG)
        b += reg("ITM_STO", DRIVER_RESULT_BASE + i)
    b += op("ITM_RTN")
    b += op("ITM_END")
    return b


def render(program_bytes):
    lines = ["PROGRAM_FILE_FORMAT", "0", "C47_program_file_version", "1", "PROGRAM", str(len(program_bytes))]
    lines += [str(byte) for byte in program_bytes]
    return "\n".join(lines) + "\n"


def generate():
    files = {name + ".p47": render(gen()) for name, gen in GENERATORS.items()}
    files["BENCHALL.p47"] = render(benchall())
    return files


def main():
    parser = argparse.ArgumentParser(description="Generate res/PROGRAMS/bench/*.p47")
    parser.add_argument("--check", action="store_true", help="verify checked-in files match the generator")
    args = parser.parse_args()

    files = generate()
    if args.check:
        stale = []
        for name, content in sorted(files.items()):
            path = OUT_DIR / name
            if not path.exists() or path.read_text() != content:
                stale.append(name)
        if stale:
            print("stale or missing: " + " ".join(stale))
            print("run: python3 tools/bench/genbench.py")
            return 1
        print(f"{len(files)} benchmark programs up to date")
        return 0

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, content in sorted(files.items()):
        (OUT_DIR / name).write_text(content)
        print(f"wrote {OUT_DIR / name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
