#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the CSV files of run_benchmark.py (and compile_time.py) into a
Markdown report with the budgets and SVG charts.

    python3 plot_results.py [results_dir]

Reads <dir>/<benchmark>_<toolchain>.csv for codec, protocol, logging,
dialogue and devices, and <dir>/compile_time.csv when it exists. Writes into
<dir>:

    transport_benchmark.md   the numbers and the budgets of the plan (question
                             14), a verdict per toolchain
    codec_throughput.svg     MB/s of every codec at 2420 bytes (encode and
                             decode), per toolchain
    round_trip.svg           the time of one exchange per case (log scale)
    devices.svg              CPU as a share of one core for 16 devices
    compile_time.svg         seconds and peak memory of a client over a table

Standard library only.
"""

import csv
import glob
import math
import os
import sys
from xml.sax.saxutils import escape

BENCHES = ["codec", "protocol", "logging", "dialogue", "devices"]
COLORS = ["#2a6fb0", "#d0702a", "#3b8f5a", "#8a4fa6"]


def read_csv(path):
    with open(path) as handle:
        lines = [line for line in handle if not line.startswith("#")]
        handle.seek(0)
        meta = [line[2:].rstrip() for line in handle if line.startswith("#")]
    return meta, list(csv.DictReader(lines))


def load_results(directory):
    """{benchmark: {toolchain: {case: row}}} and the meta lines."""
    out = {}
    meta = {}
    for bench in BENCHES:
        out[bench] = {}
        for path in sorted(glob.glob(os.path.join(directory,
                                                  bench + "_*.csv"))):
            tool = os.path.basename(path)[len(bench) + 1:-4]
            lines, rows = read_csv(path)
            out[bench][tool] = dict((row["case"], row) for row in rows)
            meta[tool] = lines
    return out, meta


def number(row, key="median"):
    return float(row[key])


# -- SVG helpers --

def svg_bars(path, title, unit, groups, series, log=False, rule=None,
             rule_label=""):
    """groups: labels; series: [(name, [value or None per group])]."""
    width, left, top, row_h = 880, 270, 54, 17
    rows = len(groups) * len(series)
    height = top + rows * row_h + len(groups) * 6 + 40
    values = [v for _, vs in series for v in vs if v is not None and v > 0]
    if rule:
        values.append(rule)
    low = min(values) if log else 0.0
    high = max(values) * 1.15
    if log:
        low = 10 ** math.floor(math.log10(low))
        high = 10 ** math.ceil(math.log10(high))

    def x_of(v):
        span = width - left - 90
        if log:
            v = max(v, low)
            return left + span * (math.log10(v) - math.log10(low)) / (
                math.log10(high) - math.log10(low))
        return left + span * v / high

    parts = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
             'font-family="sans-serif" font-size="11">' % (width, height),
             '<rect width="100%" height="100%" fill="#ffffff"/>',
             '<text x="10" y="20" font-size="14" font-weight="bold">%s</text>'
             % escape(title),
             '<text x="10" y="38" fill="#555">%s%s</text>'
             % (escape(unit), ", log scale" if log else "")]
    y = top
    for gi, group in enumerate(groups):
        parts.append('<text x="%d" y="%d" text-anchor="end">%s</text>'
                     % (left - 8, y + row_h - 5, escape(group)))
        for si, (name, vs) in enumerate(series):
            v = vs[gi]
            if v is not None and v > 0:
                parts.append(
                    '<rect x="%d" y="%d" width="%.1f" height="%d" fill="%s"/>'
                    % (left, y + 2, max(x_of(v) - left, 1), row_h - 4,
                       COLORS[si % len(COLORS)]))
                parts.append('<text x="%.1f" y="%d" fill="#333">%.3g</text>'
                             % (x_of(v) + 4, y + row_h - 5, v))
            y += row_h
        y += 6
    if rule:
        parts.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%d" '
                     'stroke="#c0392b" stroke-dasharray="4 3"/>'
                     % (x_of(rule), top - 6, x_of(rule), y))
        parts.append('<text x="%.1f" y="%d" fill="#c0392b">%s</text>'
                     % (x_of(rule) + 4, top - 8, escape(rule_label)))
    for si, (name, _) in enumerate(series):
        parts.append('<rect x="%d" y="%d" width="10" height="10" fill="%s"/>'
                     '<text x="%d" y="%d">%s</text>'
                     % (10 + si * 170, height - 22, COLORS[si % len(COLORS)],
                        24 + si * 170, height - 13, escape(name)))
    parts.append("</svg>")
    with open(path, "w") as handle:
        handle.write("\n".join(parts) + "\n")


# -- Tables --

def md_table(header, rows):
    out = ["| " + " | ".join(header) + " |",
           "| " + " | ".join("---" for _ in header) + " |"]
    for row in rows:
        out.append("| " + " | ".join(row) + " |")
    return out


def fmt(value, digits=1):
    if value is None:
        return "-"
    return ("%." + str(digits) + "f") % value


def mb_per_s(row):
    for part in row["note"].split():
        if part.startswith("MB/s="):
            return float(part[5:])
    return None


def budgets(results, compile_rows):
    """[(budget, per-toolchain list of (tool, value text, ok))]."""
    out = []
    tools = sorted(set(t for b in BENCHES for t in results[b]))

    def each(bench, case, key, convert, limit, below, text):
        verdicts = []
        for tool in tools:
            row = results[bench].get(tool, {}).get(case)
            if row is None:
                continue
            value = convert(row)
            ok = value < limit if below else value >= limit
            verdicts.append((tool, text % value, ok, row["loadavg"]))
        return verdicts

    out.append(("Log call below the threshold: under 2 ns",
                each("logging", "lazy_below_threshold", "median",
                     lambda r: number(r), 2.0, True, "%.2f ns")))
    out.append(("Dialogue round trip on mocks (async, virtual clock): under "
                "50 us",
                each("dialogue", "ping6_virtual", "median",
                     lambda r: number(r) / 1000.0, 50.0, True, "%.1f us")))
    out.append(("Dialogue round trip on mocks (sync call, runner thread): "
                "under 50 us",
                each("dialogue", "ping6_sync_thread", "median",
                     lambda r: number(r) / 1000.0, 50.0, True, "%.1f us")))
    out.append(("describe of a 100-byte frame: under 5 us",
                each("protocol", "describe_rx_100", "median",
                     lambda r: number(r) / 1000.0, 5.0, True, "%.2f us")))
    for tool in tools:
        pass
    slowest = []
    for tool in tools:
        rows = [r for c, r in results["codec"].get(tool, {}).items()
                if c.endswith("_2420_encode") or c.endswith("_2420_decode")]
        if rows:
            worst = min(rows, key=lambda r: mb_per_s(r) or 0)
            slowest.append((tool, "%s at %.0f MB/s" % (worst["case"],
                                                       mb_per_s(worst)),
                            all((mb_per_s(r) or 0) >= 100 for r in rows),
                            worst["loadavg"]))
    out.append(("Codecs at 2420 bytes: at least 100 MB/s (the slowest shown)",
                slowest))
    for case, label in (("sized_25_with_clients", "2420 bytes at 25 Hz"),
                        ("small_25_with_clients", "16 bytes at 25 Hz"),
                        ("small_20_with_clients", "16 bytes at 20 Hz"),
                        ("mixed_with_clients", "mixed loads")):
        out.append(("16 devices on one io_runner thread, %s: under 10 %% of "
                    "a core" % label,
                    each("devices", case, "median", lambda r: number(r),
                         10.0, True, "%.2f %%")))
    for tool, rows in compile_rows.items():
        base = rows.get(("O0", "headers"))
        for mode in ("syntax", "O0", "O2"):
            head = rows.get((mode, "headers"))
            big = rows.get((mode, "device_m1_20_methods"))
            if head and big:
                delta = float(big["median_s"]) - float(head["median_s"])
                out.append(("Client over the largest table adds under 2 s "
                            "(%s, %s)" % (tool, mode),
                            [(tool, "+%.2f s, %s MB" % (
                                delta, big["peak_rss_mb"]), delta < 2.0,
                                big["loadavg"])]))
    return out


def main():
    directory = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "results")
    results, meta = load_results(directory)
    compile_rows = {}
    compile_meta = []
    compile_path = os.path.join(directory, "compile_time.csv")
    if os.path.exists(compile_path):
        compile_meta, rows = read_csv(compile_path)
        for row in rows:
            compile_rows.setdefault(row["toolchain"], {})[
                (row["mode"], row["variant"])] = row
    tools = sorted(set(t for b in BENCHES for t in results[b]))
    md = ["# Transport benchmarks", ""]
    for tool in tools:
        md.append("- " + tool + ": " + "; ".join(
            m for m in meta.get(tool, []) if not m.startswith("benchmark=")))
    md.append("")

    md.append("## Budgets")
    md.append("")
    rows = []
    for text, verdicts in budgets(results, compile_rows):
        for tool, value, ok, load in verdicts:
            rows.append([text, tool, value, "met" if ok else "MISSED", load])
    md += md_table(["Budget", "Toolchain", "Measured", "Verdict", "Load"],
                   rows)
    md.append("")

    for bench in BENCHES:
        if not results[bench]:
            continue
        md.append("## " + bench)
        md.append("")
        cases = []
        for tool in tools:
            for case in results[bench].get(tool, {}):
                if case not in cases:
                    cases.append(case)
        header = ["Case", "Clock", "Unit"]
        for tool in tools:
            header += [tool + " median", tool + " min", tool + " load"]
        table = []
        for case in cases:
            first = next(results[bench][t][case] for t in tools
                         if case in results[bench].get(t, {}))
            row = [case, first["clock"], first["unit"]]
            for tool in tools:
                r = results[bench].get(tool, {}).get(case)
                row += ([fmt(number(r), 2), fmt(number(r, "min"), 2),
                         r["loadavg"]] if r else ["-", "-", "-"])
            table.append(row)
        md += md_table(header, table)
        md.append("")
        notes = []
        for case in cases:
            first = next(results[bench][t][case] for t in tools
                         if case in results[bench].get(t, {}))
            if first["note"]:
                notes.append("- `%s`: %s" % (case, first["note"]))
        md += notes
        md.append("")

    if compile_rows:
        md.append("## Compile time of protocol::client<P>")
        md.append("")
        for line in compile_meta:
            if line.startswith("compiler"):
                md.append("- " + line)
        md.append("")
        header = ["Toolchain", "Mode", "Variant", "Median s", "Min s",
                  "Peak MB", "Load"]
        table = []
        for tool, rows_ in compile_rows.items():
            for (mode, variant), r in sorted(rows_.items()):
                table.append([tool, mode, variant, r["median_s"], r["min_s"],
                              r["peak_rss_mb"], r["loadavg"]])
        md += md_table(header, table)
        md.append("")

    with open(os.path.join(directory, "transport_benchmark.md"), "w") as out:
        out.write("\n".join(md) + "\n")

    # Charts.
    codec_cases = []
    for tool in tools:
        for case in results["codec"].get(tool, {}):
            if (case.endswith("_2420_encode") or case.endswith("_2420_decode")
                    ) and case not in codec_cases:
                codec_cases.append(case)
    if codec_cases:
        svg_bars(os.path.join(directory, "codec_throughput.svg"),
                 "Codec throughput at 2420 bytes", "MB/s of payload",
                 codec_cases,
                 [(t, [mb_per_s(results["codec"][t][c])
                       if c in results["codec"].get(t, {}) else None
                       for c in codec_cases]) for t in tools],
                 rule=100, rule_label="budget 100 MB/s")
    trips = []
    for tool in tools:
        for case in results["dialogue"].get(tool, {}):
            if case not in trips:
                trips.append(case)
    if trips:
        svg_bars(os.path.join(directory, "round_trip.svg"),
                 "One exchange on mocks", "microseconds per exchange", trips,
                 [(t, [number(results["dialogue"][t][c]) / 1000.0
                       if c in results["dialogue"].get(t, {}) else None
                       for c in trips]) for t in tools],
                 log=True, rule=50, rule_label="budget 50 us")
    fleets = []
    for tool in tools:
        for case in results["devices"].get(tool, {}):
            if case not in fleets:
                fleets.append(case)
    if fleets:
        svg_bars(os.path.join(directory, "devices.svg"),
                 "16 devices on one io_runner thread",
                 "CPU as a percentage of one core (virtual clock)", fleets,
                 [(t, [number(results["devices"][t][c])
                       if c in results["devices"].get(t, {}) else None
                       for c in fleets]) for t in tools],
                 rule=10, rule_label="budget 10 %")
    if compile_rows:
        labels = []
        series = []
        for tool, rows_ in compile_rows.items():
            for mode in ("syntax", "O0", "O2"):
                labels.append("%s %s" % (tool, mode))
        variants = ["headers", "table7_one_method", "device_m1_20_methods",
                    "device_m2_15_methods"]
        for variant in variants:
            values = []
            for tool, rows_ in compile_rows.items():
                for mode in ("syntax", "O0", "O2"):
                    r = rows_.get((mode, variant))
                    values.append(float(r["median_s"]) if r else None)
            series.append((variant, values))
        svg_bars(os.path.join(directory, "compile_time.svg"),
                 "Compile time of a translation unit with a client",
                 "seconds (median)", labels, series)
    sys.stderr.write("wrote %s\n" % os.path.join(directory,
                                                 "transport_benchmark.md"))


if __name__ == "__main__":
    main()
