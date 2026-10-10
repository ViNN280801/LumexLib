#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the CSV of run_benchmark.py into a Markdown table and an SVG chart.

    python plot_results.py results/circular_buffer_benchmark.csv

Reads circular_buffer_footprint.csv from the directory of the CSV when it
exists. Writes next to the CSV:
    circular_buffer_benchmark.md         the numbers: per toolchain a table of
                                         the median time per operation of
                                         every variant and its ratio to the
                                         baseline (boost::circular_buffer
                                         C++17; the first variant present when
                                         Boost was not measured), summaries and
                                         the footprint table
    circular_buffer_benchmark_<toolchain>.svg
                                         one chart per toolchain: a dot per
                                         variant and scenario at its time
                                         relative to the baseline (log scale)

Standard library only.
"""

import csv
import math
import os
import sys
from xml.sax.saxutils import escape

VARIANT_ORDER = ["boost_cxx17", "boost_cxx11", "lumex_cxx11", "lumex_cxx17",
                 "deque_cxx17", "vector_cxx17", "vector_mod_cxx17",
                 "lumex_wrap_cxx17"]
VARIANT_LABEL = {
    "boost_cxx17": "boost C++17",
    "boost_cxx11": "boost C++11",
    "lumex_cxx11": "lumex C++11",
    "lumex_cxx17": "lumex C++17",
    "deque_cxx17": "deque C++17",
    "vector_cxx17": "vector C++17",
    "vector_mod_cxx17": "vector % C++17",
    "lumex_wrap_cxx17": "lumex compare-wrap C++17",
}
VARIANT_COLOR = {
    "boost_cxx17": "#3c9d4a",
    "boost_cxx11": "#8fd19a",
    "lumex_cxx11": "#1b6ca8",
    "lumex_cxx17": "#6fb1e0",
    "deque_cxx17": "#d9822b",
    "vector_cxx17": "#8c564b",
    "vector_mod_cxx17": "#c49c94",
    "lumex_wrap_cxx17": "#9467bd",
}
# The baseline of the ratios: the first of these that was measured.
baselineS = ["boost_cxx17", "vector_cxx17"]
# A ratio within this band of 1.0 is called "on par" in the summary.
PAR = 0.10
# A scenario whose slowest pass is this much slower than its fastest pass is
# marked as unstable between processes.
UNSTABLE = 1.25
# Pairs of variants compared with each other in the second summary.
PAIRS = [("lumex_cxx17", "boost_cxx17"), ("lumex_cxx17", "vector_cxx17"),
         ("lumex_cxx17", "deque_cxx17"), ("lumex_wrap_cxx17", "boost_cxx17"),
         ("lumex_wrap_cxx17", "lumex_cxx17"), ("lumex_cxx17", "vector_mod_cxx17"),
         ("vector_mod_cxx17", "vector_cxx17"), ("lumex_cxx11", "lumex_cxx17"),
         ("lumex_cxx11", "boost_cxx11")]


def read_benchmark(path):
    comments = []
    rows = []
    with open(path, newline="") as handle:
        lines = handle.readlines()
    body = []
    for line in lines:
        if line.startswith("#"):
            comments.append(line[1:].strip())
        else:
            body.append(line)
    for row in csv.DictReader(body):
        row["median_ns"] = float(row["median_ns"])
        row["worst_median_ns"] = float(row.get("worst_median_ns")
                                       or row["median_ns"])
        row["min_ns"] = float(row["min_ns"])
        row["max_ns"] = float(row["max_ns"])
        row["size"] = int(row["size"])
        rows.append(row)
    return comments, rows


def read_traits(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def scenario_label(scenario, size):
    return "%s (%s)" % (scenario, format(size, ",")) if size else scenario


def group(rows):
    """{toolchain: {variant: {(scenario, size): row}}} and the scenario order."""
    result = {}
    order = []
    for row in rows:
        key = (row["scenario"], row["size"])
        if key not in order:
            order.append(key)
        result.setdefault(row["toolchain"], {}).setdefault(
            row["variant"], {})[key] = row
    return result, order


def baseline_of(per_variant):
    for name in baselineS:
        if name in per_variant:
            return name
    return None


def variants_of(per_variant):
    return [v for v in VARIANT_ORDER if v in per_variant]


def pair_ratios(per_variant, numerator, denominator, order):
    """{(scenario, size): time of `numerator` / time of `denominator`}."""
    out = {}
    for key in order:
        top = per_variant[numerator].get(key)
        bottom = per_variant[denominator].get(key)
        if top is not None and bottom is not None:
            out[key] = top["median_ns"] / max(bottom["median_ns"], 1e-9)
    return out


def geometric_mean(values):
    values = list(values)
    return math.exp(sum(math.log(v) for v in values) / len(values))


def unstable(row):
    return row["worst_median_ns"] > UNSTABLE * row["median_ns"]


def format_ns(value):
    if value >= 1000:
        return "%.0f" % value
    if value >= 100:
        return "%.1f" % value
    return "%.2f" % value


def markdown(comments, grouped, order, traits):
    lines = ["# circular_buffer benchmark results", ""]
    for comment in comments:
        lines.append("- " + comment)
    lines.append("")
    lines.append("Time per operation in nanoseconds: the lowest median of "
                 "15 repetitions over all passes (the best pass), and in "
                 "parentheses the ratio to the baseline column (the first "
                 "column; below 1.00 is faster than that). The unit of an operation is the "
                 "description of the scenario. A dagger marks a value whose "
                 "slowest pass was more than %d %% slower than the best "
                 "one: the processes of that executable do not agree (code "
                 "and stack placement), so differences inside that band are "
                 "noise." % int((UNSTABLE - 1) * 100))
    lines.append("")
    for toolchain, per_variant in grouped.items():
        baseline = baseline_of(per_variant)
        if baseline is None:
            continue
        variants = variants_of(per_variant)
        lines.append("## %s" % toolchain)
        lines.append("")
        header = ["scenario", "size"] + [VARIANT_LABEL[v] for v in variants]
        lines.append("| " + " | ".join(header) + " |")
        lines.append("|" + "|".join([" --- "] * 2 + [" ---: "] * len(variants))
                     + "|")
        for key in order:
            if key not in per_variant[baseline]:
                continue
            cells = [key[0], format(key[1], ",") if key[1] else ""]
            base = per_variant[baseline][key]["median_ns"]
            for variant in variants:
                row = per_variant[variant].get(key)
                mark = "†" if row is not None and unstable(row) else ""
                if row is None:
                    cells.append("")
                elif variant == baseline:
                    cells.append(format_ns(row["median_ns"]) + mark)
                else:
                    cells.append("%s (%.2f)%s" % (
                        format_ns(row["median_ns"]),
                        row["median_ns"] / max(base, 1e-9), mark))
            lines.append("| " + " | ".join(cells) + " |")
        lines.append("")
        lines.append("Variants compared with each other (time of the first "
                     "divided by the time of the second; the first is faster "
                     "below 1.00): geometric mean over all scenarios, the "
                     "best and the worst scenario of the first, and the "
                     "number of scenarios where the first is slower or "
                     "faster by more than %d %%:" % int(PAR * 100))
        lines.append("")
        lines.append("| compared | scenarios | geometric mean | slower | "
                     "faster | best | worst |")
        lines.append("| --- | ---: | ---: | ---: | ---: | --- | --- |")
        for numerator, denominator in PAIRS:
            if numerator not in per_variant or denominator not in per_variant:
                continue
            values = pair_ratios(per_variant, numerator, denominator, order)
            if not values:
                continue
            best = min(values.items(), key=lambda item: item[1])
            worst = max(values.items(), key=lambda item: item[1])
            slower = [k for k, v in values.items() if v > 1 + PAR]
            faster = [k for k, v in values.items() if v < 1 - PAR]
            lines.append("| %s / %s | %d | %.3f | %d | %d | %s %.2fx | "
                         "%s %.2fx |" % (
                             VARIANT_LABEL[numerator],
                             VARIANT_LABEL[denominator], len(values),
                             geometric_mean(values.values()), len(slower),
                             len(faster), scenario_label(*best[0]), best[1],
                             scenario_label(*worst[0]), worst[1]))
        lines.append("")
        for numerator, denominator in PAIRS[:2]:
            if numerator not in per_variant or denominator not in per_variant:
                continue
            values = pair_ratios(per_variant, numerator, denominator, order)
            slower = sorted(((v, k) for k, v in values.items()
                             if v > 1 + PAR), reverse=True)
            faster = sorted((v, k) for k, v in values.items()
                            if v < 1 - PAR)
            lines.append("- %s is slower than %s by more than %d %% in: %s" % (
                VARIANT_LABEL[numerator], VARIANT_LABEL[denominator],
                int(PAR * 100),
                ", ".join("%s %.2fx" % (scenario_label(*k), v)
                          for v, k in slower) or "no scenario"))
            lines.append("- %s is faster than %s by more than %d %% in: %s" % (
                VARIANT_LABEL[numerator], VARIANT_LABEL[denominator],
                int(PAR * 100),
                ", ".join("%s %.2fx" % (scenario_label(*k), v)
                          for v, k in faster) or "no scenario"))
        lines.append("")
    if traits:
        lines.append("## Footprint")
        lines.append("")
        lines.append("A buffer of capacity 1 024 elements of `T`. `sizeof` "
                     "of the container object; heap memory requested from "
                     "`operator new` (bytes / calls) after construction "
                     "(empty), after 10 `push_back` and after 1 024 "
                     "`push_back`; NM: nothrow move constructible and "
                     "nothrow move assignable. `std::string` is the empty "
                     "string, so no heap memory is counted for the "
                     "characters. Sizes in bytes; the values do not depend "
                     "on the toolchain of this run (one 64-bit Linux x86-64 "
                     "data model).")
        lines.append("")
        toolchain = traits[0]["toolchain"]
        types = []
        for row in traits:
            if row["toolchain"] == toolchain and row["type"] not in types:
                types.append(row["type"])
        shown = [v for v in VARIANT_ORDER
                 if any(r["variant"] == v and r["toolchain"] == toolchain
                        for r in traits)]
        for type_name in types:
            first = [r for r in traits if r["toolchain"] == toolchain
                     and r["type"] == type_name][0]
            lines.append("`T` = `%s` (`sizeof (T)` %s)" % (
                type_name, first["sizeof_t"]))
            lines.append("")
            lines.append("| variant | sizeof (container) | empty | 10 elements "
                         "| 1 024 elements | NM (ctor/assign) |")
            lines.append("| --- | ---: | ---: | ---: | ---: | --- |")
            for variant in shown:
                match = [r for r in traits if r["toolchain"] == toolchain
                         and r["variant"] == variant
                         and r["type"] == type_name][0]
                lines.append("| %s | %s | %s / %s | %s / %s | %s / %s | %s/%s |"
                             % (VARIANT_LABEL[variant], match["sizeof_ring"],
                                match["empty_bytes"], match["empty_allocs"],
                                match["ten_bytes"], match["ten_allocs"],
                                match["full_bytes"], match["full_allocs"],
                                "Y" if match["nothrow_move_constructible"]
                                == "1" else "n",
                                "Y" if match["nothrow_move_assignable"]
                                == "1" else "n"))
            lines.append("")
    return "\n".join(lines) + "\n"


def svg_dot_chart(title, subtitle, per_variant, order, baseline, baseline_label):
    variants = [v for v in variants_of(per_variant)]
    keys = [k for k in order if k in per_variant[baseline]]
    left, top, row_h, width = 250, 70, 20, 1000
    plot_w = width - left - 30
    height = top + row_h * len(keys) + 78
    low, high = math.log(0.1), math.log(10.0)

    def x_of(ratio):
        clamped = min(max(math.log(max(ratio, 1e-9)), low), high)
        return left + (clamped - low) / (high - low) * plot_w

    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif" '
           'font-size="11">' % (width, height, width, height),
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="%d" y="22" font-size="15" font-weight="bold">%s</text>'
           % (left, escape(title)),
           '<text x="%d" y="40" fill="#555">%s</text>'
           % (left, escape(subtitle))]
    x = left
    for variant in variants:
        out.append('<circle cx="%d" cy="55" r="4.5" fill="%s"/>'
                   % (x + 5, VARIANT_COLOR[variant]))
        out.append('<text x="%d" y="59">%s</text>'
                   % (x + 14, escape(VARIANT_LABEL[variant])))
        x += 24 + 6.2 * len(VARIANT_LABEL[variant])
    for ratio in (0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0):
        gx = x_of(ratio)
        stroke = "#444" if ratio == 1.0 else "#ddd"
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%d" stroke="%s"/>'
                   % (gx, top - 6, gx, top + row_h * len(keys), stroke))
        out.append('<text x="%.1f" y="%d" text-anchor="middle" fill="#555">'
                   '%s</text>' % (gx, top + row_h * len(keys) + 14,
                                  ("%gx" % ratio)))
    out.append('<text x="%d" y="%d" text-anchor="middle" fill="#555">time '
               'relative to %s (log scale, clipped at 0.1x '
               'and 10x; left is faster)</text>'
               % (left + plot_w // 2, top + row_h * len(keys) + 34,
                  escape(baseline_label)))
    out.append('<text x="%d" y="%d" text-anchor="middle" fill="#555">a '
               'hollow dot: the processes of that executable disagree by '
               'more than %d %% (code and stack placement), read it as '
               'noise</text>'
               % (left + plot_w // 2, top + row_h * len(keys) + 50,
                  int((UNSTABLE - 1) * 100)))
    for index, key in enumerate(keys):
        y = top + index * row_h + row_h / 2
        if index % 2 == 0:
            out.append('<rect x="0" y="%.1f" width="%d" height="%d" '
                       'fill="#f6f8fa"/>' % (y - row_h / 2, width, row_h))
        out.append('<text x="%d" y="%.1f" text-anchor="end">%s</text>'
                   % (left - 8, y + 4, escape(scenario_label(*key))))
        base = per_variant[baseline][key]["median_ns"]
        for variant in variants:
            row = per_variant[variant].get(key)
            if row is None:
                continue
            ratio = row["median_ns"] / max(base, 1e-9)
            cx = x_of(ratio)
            color = VARIANT_COLOR[variant]
            if unstable(row):
                fill = 'fill="none" stroke="%s" stroke-width="1.6"' % color
            else:
                fill = 'fill="%s" fill-opacity="0.9"' % color
            tip = "%s: %s ns, %.2fx%s" % (
                VARIANT_LABEL[variant], format_ns(row["median_ns"]), ratio,
                " (unstable)" if unstable(row) else "")
            out.append('<circle cx="%.1f" cy="%.1f" r="4" %s><title>%s'
                       '</title></circle>' % (cx, y, fill, escape(tip)))
    out.append("</svg>")
    return "\n".join(out) + "\n"


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    csv_path = sys.argv[1]
    out_dir = os.path.dirname(os.path.abspath(csv_path))
    comments, rows = read_benchmark(csv_path)
    grouped, order = group(rows)
    traits_path = os.path.join(out_dir, "circular_buffer_footprint.csv")
    traits = read_traits(traits_path) if os.path.exists(traits_path) else []
    with open(os.path.join(out_dir, "circular_buffer_benchmark.md"),
              "w") as handle:
        handle.write(markdown(comments, grouped, order, traits))
    machine = next((c for c in comments if c.startswith("machine=")), "")
    for toolchain, per_variant in grouped.items():
        baseline = baseline_of(per_variant)
        if baseline is None:
            continue
        compilers = sorted({c.split("compiler=")[1].split(" library=")[0]
                            .strip('"')
                            for c in comments
                            if c.startswith("variant %s " % toolchain)})
        model = machine.split('"')[1] if '"' in machine else machine
        subtitle = "%s; %s" % (", ".join(compilers), model)
        path = os.path.join(out_dir,
                            "circular_buffer_benchmark_%s.svg" % toolchain)
        label = VARIANT_LABEL[baseline]
        with open(path, "w") as handle:
            handle.write(svg_dot_chart(
                "circular_buffer, time relative to %s (%s)"
                % (label, toolchain), subtitle, per_variant, order, baseline,
                label))
        print("wrote " + path)
    print("wrote " + os.path.join(out_dir, "circular_buffer_benchmark.md"))


if __name__ == "__main__":
    main()
