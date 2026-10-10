#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the CSV of run_benchmark.py into a Markdown table and SVG charts.

    python plot_results.py results/span_benchmark.csv [results/compile_time.csv]

Writes next to the CSV:
    span_benchmark.md                 the numbers: per toolchain a table of
                                      the median time per operation of every
                                      variant and its ratio to the span of
                                      this library at C++11, a summary, and
                                      the compile times when given
    span_benchmark_<toolchain>.svg    one chart per toolchain: a dot per
                                      variant and scenario at its time
                                      relative to lumex at C++11 (log scale)
    compile_time.svg                  the compile times, when given

Standard library only.
"""

import csv
import math
import os
import sys
from xml.sax.saxutils import escape

VARIANT_ORDER = ["lumex_cxx11", "lumex_cxx20", "std_cxx20", "boost_cxx11",
                 "boost_cxx20"]
VARIANT_LABEL = {
    "lumex_cxx11": "lumex C++11",
    "lumex_cxx20": "lumex C++20",
    "std_cxx20": "std::span C++20",
    "boost_cxx11": "boost::span C++11",
    "boost_cxx20": "boost::span C++20",
    "none_cxx11": "no span C++11",
    "none_cxx20": "no span C++20",
}
VARIANT_COLOR = {
    "lumex_cxx11": "#1b6ca8",
    "lumex_cxx20": "#6fb1e0",
    "std_cxx20": "#d9822b",
    "boost_cxx11": "#3c9d4a",
    "boost_cxx20": "#8fd19a",
    "none_cxx11": "#888888",
    "none_cxx20": "#bbbbbb",
}
BASELINE = "lumex_cxx11"
# A ratio within this band of 1.0 is called "on par" in the summary.
PAR = 0.10
# A scenario whose slowest pass is this much slower than its fastest pass is
# marked as unstable between processes.
UNSTABLE = 1.25


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


def read_compile(path):
    comments = []
    body = []
    with open(path, newline="") as handle:
        for line in handle:
            if line.startswith("#"):
                comments.append(line[1:].strip())
            else:
                body.append(line)
    rows = list(csv.DictReader(body))
    for row in rows:
        row["median_ms"] = float(row["median_ms"])
        row["min_ms"] = float(row["min_ms"])
    return comments, rows


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


def variants_of(per_variant):
    return [v for v in VARIANT_ORDER if v in per_variant]


def ratios(per_variant, variant, order):
    base = per_variant[BASELINE]
    out = {}
    for key in order:
        if key in per_variant[variant] and key in base:
            out[key] = per_variant[variant][key]["median_ns"] / max(
                base[key]["median_ns"], 1e-9)
    return out


def pair_ratios(per_variant, numerator, denominator, order):
    """{(scenario, size): time of `numerator` / time of `denominator`}."""
    out = {}
    for key in order:
        top = per_variant[numerator].get(key)
        bottom = per_variant[denominator].get(key)
        if top is not None and bottom is not None:
            out[key] = top["median_ns"] / max(bottom["median_ns"], 1e-9)
    return out


# Pairs of variants of the same C++ standard, compared with each other.
SAME_STANDARD = [("lumex_cxx20", "std_cxx20"), ("lumex_cxx20", "boost_cxx20"),
                 ("lumex_cxx11", "boost_cxx11")]


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


def markdown(comments, grouped, order, compile_rows, compile_comments):
    lines = ["# span benchmark results", ""]
    for comment in comments:
        lines.append("- " + comment)
    lines.append("")
    lines.append("Time per operation in nanoseconds: the lowest median of "
                 "15 repetitions over all passes (the best pass), and in "
                 "parentheses the ratio to the span of this library at C++11 "
                 "(below 1.00 is faster than that). The unit of an operation "
                 "is the description of the scenario. A dagger marks a value "
                 "whose slowest pass was more than %d %% slower than the "
                 "best one: the processes of that executable do not agree "
                 "(code and stack placement), so differences inside that "
                 "band are noise." % int((UNSTABLE - 1) * 100))
    lines.append("")
    for toolchain, per_variant in grouped.items():
        if BASELINE not in per_variant:
            continue
        variants = variants_of(per_variant)
        lines.append("## %s" % toolchain)
        lines.append("")
        header = ["scenario", "size"] + [VARIANT_LABEL[v] for v in variants]
        lines.append("| " + " | ".join(header) + " |")
        lines.append("|" + "|".join([" --- "] * 2 + [" ---: "] * len(variants))
                     + "|")
        for key in order:
            if key not in per_variant[BASELINE]:
                continue
            cells = [key[0], format(key[1], ",") if key[1] else ""]
            base = per_variant[BASELINE][key]["median_ns"]
            for variant in variants:
                row = per_variant[variant].get(key)
                mark = "\u2020" if row is not None and unstable(row) else ""
                if row is None:
                    cells.append("")
                elif variant == BASELINE:
                    cells.append(format_ns(row["median_ns"]) + mark)
                else:
                    cells.append("%s (%.2f)%s" % (
                        format_ns(row["median_ns"]),
                        row["median_ns"] / max(base, 1e-9), mark))
            lines.append("| " + " | ".join(cells) + " |")
        lines.append("")
        lines.append("Summary against lumex C++11 (geometric mean of the "
                     "ratio over all scenarios; scenarios slower or faster "
                     "by more than %d %%):" % int(PAR * 100))
        lines.append("")
        lines.append("| variant | scenarios | geometric mean | slower | faster |")
        lines.append("| --- | ---: | ---: | ---: | ---: |")
        for variant in variants:
            if variant == BASELINE:
                continue
            values = ratios(per_variant, variant, order)
            if not values:
                continue
            slower = [k for k, v in values.items() if v > 1 + PAR]
            faster = [k for k, v in values.items() if v < 1 - PAR]
            lines.append("| %s | %d | %.3f | %d | %d |" % (
                VARIANT_LABEL[variant], len(values),
                geometric_mean(values.values()), len(slower), len(faster)))
        lines.append("")
        lines.append("The columns of one C++ standard compared with each "
                     "other (time of the first divided by the time of the "
                     "second; the span of this library is faster below "
                     "1.00): geometric mean over all scenarios, the best "
                     "and the worst scenario of the first:")
        lines.append("")
        lines.append("| compared | scenarios | geometric mean | best | worst |")
        lines.append("| --- | ---: | ---: | --- | --- |")
        for numerator, denominator in SAME_STANDARD:
            if numerator not in per_variant or denominator not in per_variant:
                continue
            values = pair_ratios(per_variant, numerator, denominator, order)
            if not values:
                continue
            best = min(values.items(), key=lambda item: item[1])
            worst = max(values.items(), key=lambda item: item[1])
            lines.append("| %s / %s | %d | %.3f | %s %.2fx | %s %.2fx |" % (
                VARIANT_LABEL[numerator], VARIANT_LABEL[denominator],
                len(values), geometric_mean(values.values()),
                scenario_label(*best[0]), best[1],
                scenario_label(*worst[0]), worst[1]))
        lines.append("")
        for variant in variants:
            if variant == BASELINE:
                continue
            values = ratios(per_variant, variant, order)
            slower = sorted(((v, k) for k, v in values.items()
                             if v > 1 + PAR), reverse=True)
            if slower:
                lines.append("- %s is slower by more than %d %% in: %s" % (
                    VARIANT_LABEL[variant], int(PAR * 100),
                    ", ".join("%s %.2fx" % (scenario_label(*k), v)
                              for v, k in slower)))
        lines.append("")
        # Scenarios the baseline (lumex C++11) does not have: the conversions
        # to and from std::span exist for the span of this library at C++20
        # only. Absolute time and the ratio to `copy` of the same variant.
        extra = [k for k in order if k not in per_variant[BASELINE]
                 and any(k in per_variant[v] for v in variants)]
        if extra:
            lines.append("### Conversions to and from std::span (lumex C++20 "
                         "only; the ratio is to `copy` of the same variant)")
            lines.append("")
            lines.append("| scenario | size | lumex C++20 |")
            lines.append("| --- | --- | ---: |")
            copy_row = per_variant.get("lumex_cxx20", {}).get(("copy", 1024))
            for key in extra:
                row = per_variant["lumex_cxx20"].get(key)
                if row is None:
                    continue
                ratio = (" (%.2f)" % (row["median_ns"]
                                       / max(copy_row["median_ns"], 1e-9))
                         if copy_row else "")
                lines.append("| %s | %s | %s%s |" % (
                    key[0], format(key[1], ",") if key[1] else "",
                    format_ns(row["median_ns"]), ratio))
            lines.append("")
    if compile_rows:
        lines.append("## Compile time")
        lines.append("")
        for comment in compile_comments:
            lines.append("- " + comment)
        lines.append("")
        lines.append("Wall time of `-fsyntax-only -O0` of `compile_time.cpp` "
                     "(includes the span header, builds spans of an array, "
                     "a `std::array` and a `std::vector`, the subviews and "
                     "`as_bytes`), median of the repeats, in milliseconds. "
                     "`no span` is the same unit with a stub instead of a "
                     "span: the cost of the shared headers.")
        lines.append("")
        toolchains = []
        for row in compile_rows:
            if row["toolchain"] not in toolchains:
                toolchains.append(row["toolchain"])
        variants = []
        for row in compile_rows:
            if row["variant"] not in variants:
                variants.append(row["variant"])
        order_all = ["none_cxx11", "none_cxx20"] + VARIANT_ORDER
        variants.sort(key=lambda v: order_all.index(v)
                      if v in order_all else len(order_all))
        lines.append("| toolchain | " + " | ".join(VARIANT_LABEL[v]
                                                  for v in variants) + " |")
        lines.append("| --- |" + " ---: |" * len(variants))
        for toolchain in toolchains:
            cells = []
            for variant in variants:
                match = [r for r in compile_rows
                         if r["toolchain"] == toolchain
                         and r["variant"] == variant]
                cells.append("%.0f" % match[0]["median_ms"] if match else "")
            lines.append("| %s | %s |" % (toolchain, " | ".join(cells)))
        lines.append("")
    return "\n".join(lines) + "\n"


def svg_dot_chart(title, subtitle, per_variant, order):
    variants = [v for v in variants_of(per_variant)]
    keys = [k for k in order if k in per_variant[BASELINE]]
    left, top, row_h, width = 250, 70, 20, 980
    plot_w = width - left - 30
    height = top + row_h * len(keys) + 78
    low, high = math.log(0.5), math.log(2.0)

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
    # legend
    x = left
    for variant in variants:
        out.append('<circle cx="%d" cy="55" r="4.5" fill="%s"/>'
                   % (x + 5, VARIANT_COLOR[variant]))
        out.append('<text x="%d" y="59">%s</text>'
                   % (x + 14, escape(VARIANT_LABEL[variant])))
        x += 24 + 6.2 * len(VARIANT_LABEL[variant])
    # grid
    for ratio in (0.5, 0.7, 1.0, 1.4, 2.0):
        gx = x_of(ratio)
        stroke = "#444" if ratio == 1.0 else "#ddd"
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%d" stroke="%s"/>'
                   % (gx, top - 6, gx, top + row_h * len(keys), stroke))
        out.append('<text x="%.1f" y="%d" text-anchor="middle" fill="#555">'
                   '%s</text>' % (gx, top + row_h * len(keys) + 14,
                                  ("%gx" % ratio)))
    out.append('<text x="%d" y="%d" text-anchor="middle" fill="#555">time '
               'relative to lumex C++11 (log scale, clipped at 0.5x and 2x; '
               'left is faster)</text>'
               % (left + plot_w // 2, top + row_h * len(keys) + 34))
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
        base = per_variant[BASELINE][key]["median_ns"]
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


def svg_compile_chart(rows, comments):
    toolchains = []
    for row in rows:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
    variants = []
    for row in rows:
        if row["variant"] not in variants:
            variants.append(row["variant"])
    order_all = ["none_cxx11", "none_cxx20"] + VARIANT_ORDER
    variants.sort(key=lambda v: order_all.index(v)
                  if v in order_all else len(order_all))
    maximum = max(r["median_ms"] for r in rows)
    left, top, bar_h, gap, width = 140, 60, 14, 22, 900
    plot_w = width - left - 80
    height = top + len(toolchains) * (len(variants) * bar_h + gap) + 30
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif" '
           'font-size="11">' % (width, height, width, height),
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="20" y="24" font-size="15" font-weight="bold">Compile '
           'time of one translation unit (-fsyntax-only -O0), ms</text>',
           '<text x="20" y="42" fill="#555">the unit includes the span '
           'header and uses the whole API; "no span" is the same unit with '
           'a stub</text>']
    y = top
    for toolchain in toolchains:
        out.append('<text x="20" y="%d" font-weight="bold">%s</text>'
                   % (y + 12, escape(toolchain)))
        for variant in variants:
            match = [r for r in rows if r["toolchain"] == toolchain
                     and r["variant"] == variant]
            if not match:
                continue
            value = match[0]["median_ms"]
            w = value / maximum * plot_w
            out.append('<text x="%d" y="%d" text-anchor="end">%s</text>'
                       % (left - 6, y + 11, escape(VARIANT_LABEL[variant])))
            out.append('<rect x="%d" y="%d" width="%.1f" height="%d" '
                       'fill="%s"/>' % (left, y, w, bar_h - 2,
                                        VARIANT_COLOR[variant]))
            out.append('<text x="%.1f" y="%d">%.0f</text>'
                       % (left + w + 5, y + 11, value))
            y += bar_h
        y += gap
    out.append("</svg>")
    return "\n".join(out) + "\n"


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    csv_path = sys.argv[1]
    compile_path = sys.argv[2] if len(sys.argv) > 2 else None
    out_dir = os.path.dirname(os.path.abspath(csv_path))
    comments, rows = read_benchmark(csv_path)
    grouped, order = group(rows)
    compile_rows, compile_comments = [], []
    if compile_path and os.path.exists(compile_path):
        compile_comments, compile_rows = read_compile(compile_path)
    with open(os.path.join(out_dir, "span_benchmark.md"), "w") as handle:
        handle.write(markdown(comments, grouped, order, compile_rows,
                              compile_comments))
    machine = next((c for c in comments if c.startswith("machine=")), "")
    for toolchain, per_variant in grouped.items():
        if BASELINE not in per_variant:
            continue
        compilers = sorted({c.split("compiler=")[1].split(" library=")[0]
                            .strip('"')
                            for c in comments
                            if c.startswith("variant %s " % toolchain)})
        model = machine.split('"')[1] if '"' in machine else machine
        subtitle = "%s; %s" % (", ".join(compilers), model)
        path = os.path.join(out_dir, "span_benchmark_%s.svg" % toolchain)
        with open(path, "w") as handle:
            handle.write(svg_dot_chart(
                "span, time relative to lumex C++11 (%s)" % toolchain,
                subtitle, per_variant, order))
        print("wrote " + path)
    if compile_rows:
        path = os.path.join(out_dir, "compile_time.svg")
        with open(path, "w") as handle:
            handle.write(svg_compile_chart(compile_rows, compile_comments))
        print("wrote " + path)
    print("wrote " + os.path.join(out_dir, "span_benchmark.md"))


if __name__ == "__main__":
    main()
