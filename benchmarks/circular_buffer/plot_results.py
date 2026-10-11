#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the CSV of run_benchmark.py into a Markdown table and an SVG chart.

    python plot_results.py results/circular_buffer_benchmark.csv \\
                           [results/compile_time.csv]

Reads circular_buffer_footprint.csv from the directory of the CSV when it
exists. Writes next to the CSV:
    circular_buffer_benchmark.md         the numbers: per toolchain a table of
                                         the median time per operation of
                                         every variant and its ratio to the
                                         baseline (boost::circular_buffer
                                         C++17; the first variant present when
                                         Boost was not measured), summaries and
                                         the footprint table, and the
                                         compile times when given
    circular_buffer_benchmark_<toolchain>.svg
                                         one chart per toolchain: a dot per
                                         variant and scenario at its time
                                         relative to the baseline (log scale)
    circular_buffer_footprint.svg        the size of the container object and
                                         the heap memory of a buffer of
                                         1 024 elements per element type, a
                                         bar per variant family
                                         (circular_buffer_footprint_
                                         <toolchain>.svg for every further
                                         toolchain)
    compile_time.svg                     the compile times, when given

Standard library only.
"""

import csv
import math
import os
import sys
from xml.sax.saxutils import escape

VARIANT_ORDER = ["boost_cxx17", "boost_cxx11", "lumex_cxx11", "lumex_cxx17",
                 "deque_cxx17", "vector_cxx17", "vector_mod_cxx17"]
VARIANT_LABEL = {
    "boost_cxx17": "boost C++17",
    "boost_cxx11": "boost C++11",
    "lumex_cxx11": "lumex C++11",
    "lumex_cxx17": "lumex C++17",
    "deque_cxx17": "deque C++17",
    "vector_cxx17": "vector C++17",
    "vector_mod_cxx17": "vector % C++17",
    "none_cxx11": "no ring C++11",
    "none_cxx17": "no ring C++17",
}
VARIANT_COLOR = {
    "boost_cxx17": "#3c9d4a",
    "boost_cxx11": "#8fd19a",
    "lumex_cxx11": "#1b6ca8",
    "lumex_cxx17": "#6fb1e0",
    "deque_cxx17": "#d9822b",
    "vector_cxx17": "#8c564b",
    "vector_mod_cxx17": "#c49c94",
    "none_cxx11": "#888888",
    "none_cxx17": "#bbbbbb",
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
         ("lumex_cxx17", "deque_cxx17"), ("lumex_cxx17", "vector_mod_cxx17"),
         ("vector_mod_cxx17", "vector_cxx17"), ("lumex_cxx11", "lumex_cxx17"),
         ("lumex_cxx11", "boost_cxx11")]
COMPILE_MODES = [("include", "the header is included and one object is "
                  "declared"),
                 ("use", "the members of the scenarios are used: "
                  "construction, `size`, `capacity`, `push_back`, "
                  "`pop_front`, `front`, `back`, `operator[]`, iteration, "
                  "`clear`, copy and move, `std::vector` of rings")]


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


def markdown(comments, grouped, order, traits, compile_rows=(),
             compile_comments=()):
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
        toolchains = []
        for row in traits:
            if row["toolchain"] not in toolchains:
                toolchains.append(row["toolchain"])

        def values_of(toolchain):
            return [{k: v for k, v in r.items() if k != "toolchain"}
                    for r in traits if r["toolchain"] == toolchain]

        # the same values in every toolchain: one set of tables is enough
        same = all(values_of(t) == values_of(toolchains[0])
                   for t in toolchains)
        lines.append("A buffer of capacity 1 024 elements of `T`. `sizeof` "
                     "of the container object; heap memory requested from "
                     "`operator new` (bytes / calls) after construction "
                     "(empty), after 10 `push_back` and after 1 024 "
                     "`push_back`; NM: nothrow move constructible and "
                     "nothrow move assignable. `std::string` is the empty "
                     "string, so no heap memory is counted for the "
                     "characters. Sizes in bytes; "
                     + ("the values do not depend on the toolchain of this "
                        "run (one 64-bit Linux x86-64 data model)."
                        if same else
                        "one set of tables per toolchain (the values of the "
                        "toolchains differ)."))
        lines.append("")
        for toolchain in (toolchains[:1] if same else toolchains):
            if not same:
                lines.append("### Toolchain %s" % toolchain)
                lines.append("")
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
                lines.append("| variant | sizeof (container) | empty | "
                             "10 elements | 1 024 elements | "
                             "NM (ctor/assign) |")
                lines.append("| --- | ---: | ---: | ---: | ---: | --- |")
                for variant in shown:
                    match = [r for r in traits if r["toolchain"] == toolchain
                             and r["variant"] == variant
                             and r["type"] == type_name][0]
                    lines.append("| %s | %s | %s / %s | %s / %s | %s / %s "
                                 "| %s/%s |"
                                 % (VARIANT_LABEL[variant],
                                    match["sizeof_ring"],
                                    match["empty_bytes"],
                                    match["empty_allocs"],
                                    match["ten_bytes"], match["ten_allocs"],
                                    match["full_bytes"],
                                    match["full_allocs"],
                                    "Y" if match["nothrow_move_constructible"]
                                    == "1" else "n",
                                    "Y" if match["nothrow_move_assignable"]
                                    == "1" else "n"))
                lines.append("")
    if compile_rows:
        lines.append("## Compile time")
        lines.append("")
        for comment in compile_comments:
            lines.append("- " + comment)
        lines.append("")
        lines.append("Wall time of `-fsyntax-only -O0` of `compile_time.cpp`, "
                     "median of the repeats, in milliseconds. `no ring` is "
                     "the same unit with a stub instead of a ring: the cost "
                     "of the shared headers (`<deque>`, `<string>`, "
                     "`<vector>`). The units of `deque` and `vector` include "
                     "the ring classes of `bench_circular_buffer_impl.hpp`, "
                     "which are part of what they cost.")
        lines.append("")
        toolchains = []
        for row in compile_rows:
            if row["toolchain"] not in toolchains:
                toolchains.append(row["toolchain"])
        variants = compile_variants(compile_rows)
        for mode, text in COMPILE_MODES:
            lines.append("Unit `%s`: %s." % (mode, text))
            lines.append("")
            lines.append("| toolchain | " + " | ".join(VARIANT_LABEL[v]
                                                      for v in variants)
                         + " |")
            lines.append("| --- |" + " ---: |" * len(variants))
            for toolchain in toolchains:
                cells = []
                for variant in variants:
                    match = [r for r in compile_rows
                             if r["toolchain"] == toolchain
                             and r["variant"] == variant
                             and r["mode"] == mode]
                    cells.append("%.0f" % match[0]["median_ms"] if match
                                 else "")
                lines.append("| %s | %s |" % (toolchain, " | ".join(cells)))
            lines.append("")
    return "\n".join(lines) + "\n"


def compile_variants(rows):
    variants = []
    for row in rows:
        if row["variant"] not in variants:
            variants.append(row["variant"])
    order_all = ["none_cxx11", "none_cxx17"] + VARIANT_ORDER
    variants.sort(key=lambda v: order_all.index(v)
                  if v in order_all else len(order_all))
    return variants


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


def footprint_families(rows):
    """The variants of one toolchain grouped by identical footprint.

    Returns the signatures in order, {signature: [variants]} and the types.
    """
    seen = {}
    columns = []
    for variant in [v for v in VARIANT_ORDER
                    if any(r["variant"] == v for r in rows)]:
        sig = tuple(tuple(r[key] for key in (
            "type", "sizeof_ring", "empty_bytes", "empty_allocs",
            "ten_bytes", "ten_allocs", "full_bytes", "full_allocs",
            "nothrow_move_constructible", "nothrow_move_assignable"))
            for r in rows if r["variant"] == variant)
        if sig in seen:
            seen[sig].append(variant)
        else:
            seen[sig] = [variant]
            columns.append(sig)
    for names in seen.values():
        # the variants of this library first: their color names the group
        names.sort(key=lambda name: not name.startswith("lumex"))
    types = []
    for r in rows:
        if r["type"] not in types:
            types.append(r["type"])
    return columns, seen, types


def allocations(count):
    return "%s allocation%s" % (count, "" if count == "1" else "s")


def svg_footprint_chart(rows, subtitle):
    """The size of the container object, and the heap memory of a buffer of
    1 024 elements per element type: a bar per family of variants that
    request the same memory."""
    columns, seen, types = footprint_families(rows)
    size_of_t = {r["type"]: r["sizeof_t"] for r in rows}
    bar_h, gap = 11, 14
    left, top, width = 250, 70, 1000
    plot_w = 420
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif" '
           'font-size="11">' % (width, 0, width, 0),
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="%d" y="22" font-size="15" font-weight="bold">'
           'circular_buffer, footprint of a buffer of 1 024 elements</text>'
           % left,
           '<text x="%d" y="40" fill="#555">%s</text>'
           % (left, escape(subtitle))]
    x = left
    for sig in columns:
        label = ", ".join(VARIANT_LABEL[n] for n in seen[sig])
        out.append('<rect x="%d" y="49" width="10" height="10" fill="%s"/>'
                   % (x, VARIANT_COLOR[seen[sig][0]]))
        out.append('<text x="%d" y="58">%s</text>' % (x + 15, escape(label)))
        x += 34 + 6.2 * len(label)

    def bars(y_top, values, limit, text_of, title_of):
        """One bar per family at y_top; returns the height used."""
        for number, sig in enumerate(columns):
            value = values(sig)
            if value is None:
                continue
            y = y_top + number * bar_h
            w = value / limit * plot_w
            out.append('<rect x="%d" y="%.1f" width="%.1f" height="%d" '
                       'fill="%s"><title>%s</title></rect>'
                       % (left, y, w, bar_h - 2,
                          VARIANT_COLOR[seen[sig][0]],
                          escape(title_of(sig))))
            out.append('<text x="%.1f" y="%.1f" fill="#222">%s</text>'
                       % (left + w + 5, y + bar_h - 3, escape(text_of(sig))))
        return len(columns) * bar_h

    def of_type(sig, type_name):
        return [c for c in sig if c[0] == type_name][0]

    # block 1: sizeof of the container object (the same for every type)
    y = top + 4
    out.append('<text x="20" y="%d" font-weight="bold">sizeof of the '
               'container object</text>' % (y + 10))

    def ring_size(sig):
        return int(sig[0][1])

    def ring_text(sig):
        flags = {(c[8], c[9]) for c in sig}
        move = ("nothrow move: construct %s, assign %s"
                % tuple("yes" if f == "1" else "no" for f in sig[0][8:10])
                if len(flags) == 1 else "")
        sizes = {c[1] for c in sig}
        return "%s B%s" % (sig[0][1] if len(sizes) == 1
                           else "%s to %s" % (min(sizes, key=int),
                                              max(sizes, key=int)),
                           "; " + move if move else "")
    bars(y, ring_size, 100, ring_text,
         lambda sig: "%s: %s" % (", ".join(VARIANT_LABEL[n]
                                           for n in seen[sig]),
                                 ring_text(sig)))
    y += len(columns) * bar_h + 26
    # block 2: heap memory of a full buffer per element type
    out.append('<text x="20" y="%d" font-weight="bold">heap memory '
               'requested after 1 024 push_back, per element type</text>'
               % (y + 10))
    y += 22
    group_h = len(columns) * bar_h + gap
    # the axis ends at the first multiple of 8 192 bytes above the largest
    limit = -(-max(int(r["full_bytes"]) for r in rows) // 8192) * 8192
    for index in range(0, len(types), 2):
        out.append('<rect x="0" y="%.1f" width="%d" height="%d" '
                   'fill="#f6f8fa"/>' % (y + index * group_h - gap / 2,
                                         width, group_h))
    for tick in range(0, limit + 1, 8192):
        gx = left + tick / limit * plot_w
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%d" '
                   'stroke="#ddd"/>' % (gx, y - 6, gx,
                                        y + group_h * len(types) - gap + 6))
        out.append('<text x="%.1f" y="%d" text-anchor="middle" '
                   'fill="#555">%s</text>'
                   % (gx, y + group_h * len(types) - gap + 20,
                      format(tick, ",")))
    for index, type_name in enumerate(types):
        y0 = y + index * group_h
        out.append('<text x="%d" y="%.1f" text-anchor="end">%s (sizeof %s B)'
                   '</text>' % (left - 8, y0 + len(columns) * bar_h / 2 + 4,
                                escape(type_name), size_of_t[type_name]))

        def full(sig, type_name=type_name):
            return int(of_type(sig, type_name)[6])

        def full_text(sig, type_name=type_name):
            c = of_type(sig, type_name)
            return ("full: %s B in %s; empty: %s B in %s"
                    % (format(int(c[6]), ","), allocations(c[7]),
                       format(int(c[2]), ","), allocations(c[3])))
        bars(y0, full, limit, full_text,
             lambda sig, f=full_text: "%s: %s" % (
                 ", ".join(VARIANT_LABEL[n] for n in seen[sig]), f(sig)))
    end = y + group_h * len(types) - gap + 44
    out.append('<text x="%d" y="%d" text-anchor="middle" fill="#555">bytes '
               'requested from operator new, and the number of '
               'allocations</text>' % (left + plot_w // 2, end - 8))
    height = end + 6
    out[0] = out[0].replace('height="0"', 'height="%d"' % height).replace(
        'viewBox="0 0 %d 0"' % width, 'viewBox="0 0 %d %d"' % (width, height))
    out.append("</svg>")
    return "\n".join(out) + "\n"


def svg_compile_chart(rows):
    toolchains = []
    for row in rows:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
    variants = compile_variants(rows)
    modes = [m for m, _ in COMPILE_MODES if any(r["mode"] == m for r in rows)]
    maximum = max(r["median_ms"] for r in rows)
    left, top, bar_h, gap, width = 190, 60, 14, 22, 900
    plot_w = width - left - 80
    height = (top + len(toolchains) * len(modes)
              * (len(variants) * bar_h + gap) + 30)
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif" '
           'font-size="11">' % (width, height, width, height),
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="20" y="24" font-size="15" font-weight="bold">Compile '
           'time of one translation unit (-fsyntax-only -O0), ms</text>',
           '<text x="20" y="42" fill="#555">include: the header and one '
           'object; use: the members of the scenarios; "no ring" is the '
           'same unit with a stub</text>']
    y = top
    for toolchain in toolchains:
        for mode in modes:
            out.append('<text x="20" y="%d" font-weight="bold">%s, %s</text>'
                       % (y + 12, escape(toolchain), mode))
            for variant in variants:
                match = [r for r in rows if r["toolchain"] == toolchain
                         and r["variant"] == variant and r["mode"] == mode]
                if not match:
                    continue
                value = match[0]["median_ms"]
                w = value / maximum * plot_w
                out.append('<text x="%d" y="%d" text-anchor="end">%s</text>'
                           % (left - 6, y + 11,
                              escape(VARIANT_LABEL[variant])))
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
    traits_path = os.path.join(out_dir, "circular_buffer_footprint.csv")
    traits = read_traits(traits_path) if os.path.exists(traits_path) else []
    with open(os.path.join(out_dir, "circular_buffer_benchmark.md"),
              "w") as handle:
        handle.write(markdown(comments, grouped, order, traits, compile_rows,
                              compile_comments))
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
    toolchains = []
    for row in traits:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
    for number, toolchain in enumerate(toolchains):
        compilers = sorted({c.split("compiler=")[1].split(" library=")[0]
                            .strip('"') + " " + c.split("library=")[1]
                            .split(" cplusplus")[0].strip('"')
                            for c in comments
                            if c.startswith("variant %s " % toolchain)})
        subtitle = "%s; 64-bit Linux, x86-64" % ", ".join(compilers)
        name = "circular_buffer_footprint.svg" if number == 0 else (
            "circular_buffer_footprint_%s.svg" % toolchain)
        path = os.path.join(out_dir, name)
        with open(path, "w") as handle:
            handle.write(svg_footprint_chart(
                [r for r in traits if r["toolchain"] == toolchain],
                subtitle))
        print("wrote " + path)
    if compile_rows:
        path = os.path.join(out_dir, "compile_time.svg")
        with open(path, "w") as handle:
            handle.write(svg_compile_chart(compile_rows))
        print("wrote " + path)
    print("wrote " + os.path.join(out_dir, "circular_buffer_benchmark.md"))


if __name__ == "__main__":
    main()
