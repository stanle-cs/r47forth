#!/usr/bin/env python3
"""Draw the runs as bar charts into one PDF: a page for the speed and a page for the depth.

    python3 plotfnkind.py

Forty groups, one per case, with a bar per run in the order the runs were taken. Speed is on a
logarithmic scale because the cases span seven orders of magnitude, from a stack roll to a solve.
Depth is linear and only the runs from a STACK_WATERMARK build have it.

No plotting library is installed and the sandbox will not let one be, so the few operators a bar
chart needs are written straight into the file. Nothing here is generated from anything but the
runs themselves.
"""

import math
import os
import sys

import checkfnkind as ck

PAGE_W, PAGE_H = 1190.0, 842.0                       # A3 landscape, points
MARGIN_L, MARGIN_R, MARGIN_T, MARGIN_B = 62.0, 24.0, 86.0, 132.0

# The runs are found, not listed: every run folder is grouped by the calculator its folder name
# names, and ordered by the clock stamp the calculator itself put on the file. One page of speed and
# one of depth per machine, so each bar is that machine's next run and the group reads left to right
# in time. A machine with no depths anywhere gets no depth page.
MACHINES = ["DM42n", "DM42", "SIM"]

# Successive runs, oldest to newest. Enough for the runs there are; it wraps if ever exceeded.
SHADES = [(0.72, 0.80, 0.90), (0.45, 0.62, 0.82), (0.22, 0.42, 0.68), (0.09, 0.22, 0.44),
          (0.06, 0.14, 0.30)]

SPEED_FLOOR = 0.01          # ms: below this a reading is the tick resolution, not a measurement


def escape(text):
    return text.replace("\\", r"\\").replace("(", r"\(").replace(")", r"\)")


class Canvas:
    """Just enough of a drawing surface: filled rectangles, lines, and text upright or turned."""

    def __init__(self):
        self.parts = []

    def rect(self, x, y, w, h, colour):
        if h <= 0:
            return
        self.parts.append("%.3f %.3f %.3f rg %.2f %.2f %.2f %.2f re f" % (colour + (x, y, w, h)))

    def line(self, x1, y1, x2, y2, grey=0.0, width=0.5):
        self.parts.append("%.3f G %.2f w %.2f %.2f m %.2f %.2f l S" % (grey, width, x1, y1, x2, y2))

    def text(self, x, y, s, size=8.0, grey=0.0, turned=False):
        self.parts.append("BT %.3f g /F1 %.1f Tf" % (grey, size))
        if turned:
            self.parts.append("0 1 -1 0 %.2f %.2f Tm" % (x, y))
        else:
            self.parts.append("1 0 0 1 %.2f %.2f Tm" % (x, y))
        self.parts.append("(%s) Tj ET" % escape(s))

    def stream(self):
        return "\n".join(self.parts)


def write_pdf(path, pages):
    """pages: a list of content streams, all the same size."""
    objects, out = [], []
    n_pages = len(pages)
    kids = " ".join("%d 0 R" % (4 + i * 2) for i in range(n_pages))
    objects.append("<< /Type /Catalog /Pages 2 0 R >>")
    objects.append("<< /Type /Pages /Kids [%s] /Count %d >>" % (kids, n_pages))
    objects.append("<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
    for stream in pages:
        objects.append("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 %.0f %.0f] /Resources "
                       "<< /Font << /F1 3 0 R >> >> /Contents %d 0 R >>"
                       % (PAGE_W, PAGE_H, len(objects) + 2))
        objects.append("<< /Length %d >>\nstream\n%s\nendstream" % (len(stream), stream))

    out.append("%PDF-1.4\n")
    offsets = []
    for index, body in enumerate(objects, start=1):
        offsets.append(sum(len(p) for p in out))
        out.append("%d 0 obj\n%s\nendobj\n" % (index, body))
    start = sum(len(p) for p in out)
    out.append("xref\n0 %d\n0000000000 65535 f \n" % (len(objects) + 1))
    for offset in offsets:
        out.append("%010d 00000 n \n" % offset)
    out.append("trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n"
               % (len(objects) + 1, start))
    with open(path, "wb") as handle:
        handle.write("".join(out).encode("latin-1"))


def machine_of(folder):
    """The calculator a run folder names. Longest name first, so DM42n is not read as DM42."""
    for name in sorted(MACHINES, key=len, reverse=True):
        if name.lower() in folder.lower():
            return name
    return "other"


def stamp_of(folder):
    """The clock stamp the calculator wrote on the file, as the date and time it was taken."""
    names = sorted(n for n in os.listdir(folder) if n.endswith(".REGS.TSV"))
    raw = names[0].split(".")[0] if names else ""
    if len(raw) >= 13 and raw[8] == "-":
        return "%s-%s-%s %s:%s" % (raw[0:4], raw[4:6], raw[6:8], raw[9:11], raw[11:13])
    return raw


def runs_by_machine():
    """Every run, grouped by calculator and ordered by when it was taken."""
    grouped = {}
    for folder in ck.all_runs():
        grouped.setdefault(machine_of(folder), []).append(folder)
    for machine in grouped:
        grouped[machine].sort(key=stamp_of)
    return grouped


def gather(folders, value_of):
    """One entry per run: its time, its colour, and the values it measured."""
    series, names = [], {}
    for index, folder in enumerate(folders):
        run = ck.collect(folder)
        base = ck.overhead(run)
        values = {}
        for case in run["cases"]:
            names[case["number"]] = case["name"]          # the newest run wins, being processed last
            v = value_of(case, base)
            if v is not None:
                values[case["number"]] = (v, case["name"])
        series.append((stamp_of(folder), SHADES[index % len(SHADES)], values))
    return series, names


def chart(title, subtitle, series, names, logarithmic, unit, floor=None):
    c = Canvas()
    numbers = sorted(names)
    plot_w = PAGE_W - MARGIN_L - MARGIN_R
    plot_h = PAGE_H - MARGIN_T - MARGIN_B
    x0, y0 = MARGIN_L, MARGIN_B

    # A case number that meant something else in an older run is not the same measurement, so it is
    # left out rather than drawn beside its namesake. Case 40 was XFN before it was HYPERB.
    dropped = 0
    for _label, _colour, vals in series:
        for number in list(vals):
            if vals[number][1] != names.get(number):
                del vals[number]
                dropped += 1

    everything = [v for _l, _c, vals in series for v, _n in vals.values()]
    if logarithmic:
        everything = [max(v, floor) for v in everything if v > 0]
        lo = math.floor(math.log10(min(everything)))
        hi = math.ceil(math.log10(max(everything)))
        to_y = lambda v: y0 + plot_h * (math.log10(max(v, floor)) - lo) / (hi - lo)
        ticks = [(10.0 ** e, "%g" % (10.0 ** e)) for e in range(int(lo), int(hi) + 1)]
    else:
        hi = max(everything) * 1.05
        lo = 0.0
        to_y = lambda v: y0 + plot_h * (v - lo) / (hi - lo)
        step = 2000
        ticks = [(t, "%d" % t) for t in range(0, int(hi) + step, step)]

    for value, label in ticks:                                     # gridlines and the scale
        y = to_y(value)
        if y > y0 + plot_h:
            continue
        c.line(x0, y, x0 + plot_w, y, grey=0.82, width=0.4)
        c.text(x0 - 6 - 5.0 * len(label), y - 3, label, size=8, grey=0.35)

    group_w = plot_w / len(numbers)
    bar_w = group_w * 0.78 / len(series)
    for index, number in enumerate(numbers):
        gx = x0 + index * group_w
        if index % 2:                                              # a faint band, so the eye keeps its place
            c.rect(gx, y0, group_w, plot_h, (0.965, 0.965, 0.965))
        for s, (_label, colour, values) in enumerate(series):
            found = values.get(number)
            if found is None:
                continue
            v = found[0]
            if logarithmic and v <= 0:
                continue
            bx = gx + group_w * 0.11 + s * bar_w
            c.rect(bx, y0, bar_w * 0.88, to_y(v) - y0, colour)
        label = "%s %s" % (number, names[number])
        # Turned text advances upward, so start low enough that it ends just under the axis.
        c.text(gx + group_w / 2 - 3, y0 - 9 - 4.0 * len(label), label, size=7.4,
               grey=0.15, turned=True)

    c.line(x0, y0, x0 + plot_w, y0, grey=0.2, width=0.8)
    c.line(x0, y0, x0, y0 + plot_h, grey=0.2, width=0.8)

    c.text(MARGIN_L, PAGE_H - 34, title, size=15)
    c.text(MARGIN_L, PAGE_H - 50, subtitle, size=9, grey=0.35)
    c.text(MARGIN_L - 46, y0 + plot_h + 8, unit, size=8, grey=0.35)

    if dropped:
        c.text(MARGIN_L, PAGE_H - 62,
               "%d bar left out: that case number named a different case in an older run" % dropped
               if dropped == 1 else
               "%d bars left out: those case numbers named different cases in older runs" % dropped,
               size=8, grey=0.45)

    kx = MARGIN_L                                                  # the key, along the top
    for label, colour, _values in series:
        c.rect(kx, PAGE_H - 80, 9, 9, colour)
        c.text(kx + 13, PAGE_H - 78, label, size=8.5, grey=0.15)
        kx += 22 + 4.9 * len(label)
    return c.stream()


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    grouped = runs_by_machine()
    pages, made = [], []

    for machine in MACHINES:
        folders = grouped.get(machine, [])
        if not folders:
            continue

        speed, names = gather(folders, lambda case, base: ck.per_call_ms(case, base))
        speed = [x for x in speed if x[2]]
        if speed:
            pages.append(chart(
                "%s, time per call" % machine,
                "One bar per run, oldest on the left, marked with the time it was taken. Logarithmic. A bar "
                "at the bottom of the scale is at or below %g ms, the tick resolution rather than a "
                "measurement. Case 01 is the empty control and is subtracted from the rest." % SPEED_FLOOR,
                speed, names, True, "ms per call", floor=SPEED_FLOOR))
            made.append("%s speed, %d runs" % (machine, len(speed)))

        depth, dnames = gather(
            folders, lambda case, base: case["stack"] if case["status"] == ck.STATUS_MEASURED else None)
        depth = [x for x in depth if x[2]]
        if depth:
            pages.append(chart(
                "%s, working memory used" % machine,
                "One bar per run, oldest on the left, marked with the time it was taken. Bytes below the "
                "reference point, from one call. Only a reading with STCKST 0 is drawn, so a missing bar is "
                "a case that run could not resolve.",
                depth, dnames, False, "bytes"))
            made.append("%s memory, %d runs" % (machine, len(depth)))

    write_pdf(os.path.join(here, "charts.pdf"), pages)
    print("wrote charts.pdf, %d pages" % len(pages))
    for line in made:
        print("  %s" % line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
