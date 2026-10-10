#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the CSV files of run_benchmark.py into Markdown tables and SVG
charts.

    python plot_results.py results/expected_benchmark.csv results/sizes.csv \\
                           [results/compile_time.csv]

Writes next to the CSV:
    expected_benchmark.md        the numbers: per toolchain the median time per
                                 operation of every variant with its ratio to
                                 std::expected at C++23, the layers of this
                                 library against each other, the sizes and
                                 properties of the types, the compile times
    expected_benchmark_<toolchain>.svg
                                 a dot per variant and scenario at its time
                                 relative to std::expected (log scale)
    expected_layers_<toolchain>.svg
                                 the same for the layers of this library at
                                 C++14, 17, 20 and 23 against C++11
    compile_time.svg             the compile times, when given

Standard library only (Python 3.7 or later).
"""

import csv
import math
import os
import sys
from xml.sax.saxutils import escape

VARIANT_ORDER = ["std_cxx23", "lumex_cxx23", "lumex_cxx20", "lumex_cxx17",
                 "lumex_cxx14", "lumex_cxx11", "outcome_cxx17"]
VARIANT_LABEL = {
    "std_cxx23": "std::expected C++23",
    "lumex_cxx23": "lumex C++23",
    "lumex_cxx20": "lumex C++20",
    "lumex_cxx17": "lumex C++17",
    "lumex_cxx14": "lumex C++14",
    "lumex_cxx11": "lumex C++11",
    "outcome_cxx17": "Outcome C++17",
    "none_cxx11": "no header C++11",
    "none_cxx23": "no header C++23",
}
VARIANT_COLOR = {
    "std_cxx23": "#d9822b",
    "lumex_cxx23": "#0b4f8a",
    "lumex_cxx20": "#2f7fc1",
    "lumex_cxx17": "#6fb1e0",
    "lumex_cxx14": "#a9d1ee",
    "lumex_cxx11": "#1b9e77",
    "outcome_cxx17": "#8c564b",
    "none_cxx11": "#888888",
    "none_cxx23": "#bbbbbb",
}
BASELINE = "std_cxx23"
LAYER_BASELINE = "lumex_cxx11"
# A scenario whose slowest pass is this much slower than its fastest pass is
# marked as unstable between processes.
UNSTABLE = 1.25
# A ratio within this band of 1.0 is called "on par" in the summary.
PAR = 0.10
GROUP_ORDER = ["construct", "copy", "move", "assign", "access", "monadic",
               "return"]


def read_comments_and_rows(path):
    comments = []
    body = []
    with open(path, newline="") as handle:
        for line in handle:
            if line.startswith("#"):
                comments.append(line[1:].strip())
            else:
                body.append(line)
    return comments, list(csv.DictReader(body))


def read_benchmark(path):
    comments, rows = read_comments_and_rows(path)
    for row in rows:
        for field in ("median_ns", "pass_min_ns", "pass_max_ns", "min_ns",
                      "max_ns"):
            row[field] = float(row[field])
    return comments, rows


def group_rows(rows):
    """{toolchain: {variant: {scenario: row}}} and the scenario order."""
    result = {}
    order = []
    for row in rows:
        if row["scenario"] not in order:
            order.append(row["scenario"])
        result.setdefault(row["toolchain"], {}).setdefault(
            row["variant"], {})[row["scenario"]] = row
    return result, order


def variants_of(per_variant):
    return [v for v in VARIANT_ORDER if v in per_variant]


def baseline_of(per_variant):
    return BASELINE if BASELINE in per_variant else LAYER_BASELINE


def unstable(row):
    return row["pass_max_ns"] > UNSTABLE * row["pass_min_ns"]


def format_ns(value):
    if value >= 1000:
        return "%.0f" % value
    if value >= 100:
        return "%.1f" % value
    return "%.2f" % value


def geometric_mean(values):
    values = list(values)
    return math.exp(sum(math.log(v) for v in values) / len(values))


def ratio(per_variant, variant, scenario, base):
    row = per_variant[variant].get(scenario)
    top = per_variant[base].get(scenario)
    if row is None or top is None:
        return None
    return row["median_ns"] / max(top["median_ns"], 1e-9)


def ordered(order, per_variant, base):
    names = [s for s in order if s in per_variant[base]]
    group_of = {s: per_variant[base][s]["group"] for s in names}
    return sorted(names, key=lambda s: (GROUP_ORDER.index(group_of[s])
                                        if group_of[s] in GROUP_ORDER
                                        else len(GROUP_ORDER),
                                        names.index(s)))


def markdown(comments, grouped, order, size_rows, compile_rows,
             compile_comments):
    lines = ["# expected benchmark results", ""]
    for comment in comments:
        lines.append("- " + comment)
    lines.append("")
    lines.append("Time per operation in nanoseconds: the median of the "
                 "medians of the passes (each is the median of 15 "
                 "repetitions), and in parentheses the ratio to the baseline "
                 "(below 1.00 is faster than the baseline). The unit of an "
                 "operation is the description of the scenario. A dagger "
                 "marks a value whose slowest pass median was more than "
                 "%d %% above the fastest one: the processes of that "
                 "executable do not agree, so differences inside that band "
                 "are noise." % int((UNSTABLE - 1) * 100))
    lines.append("")
    for toolchain, per_variant in grouped.items():
        variants = variants_of(per_variant)
        base = baseline_of(per_variant)
        names = ordered(order, per_variant, base)
        lines.append("## %s: against %s" % (toolchain, VARIANT_LABEL[base]))
        lines.append("")
        header = ["scenario", "description"] + [VARIANT_LABEL[v]
                                                for v in variants]
        lines.append("| " + " | ".join(header) + " |")
        lines.append("|" + "|".join([" --- "] * 2 + [" ---: "]
                                     * len(variants)) + "|")
        for name in names:
            cells = [name, per_variant[base][name]["description"]]
            for variant in variants:
                row = per_variant[variant].get(name)
                if row is None:
                    cells.append("")
                    continue
                mark = "†" if unstable(row) else ""
                if variant == base:
                    cells.append(format_ns(row["median_ns"]) + mark)
                else:
                    cells.append("%s (%.2f)%s" % (
                        format_ns(row["median_ns"]),
                        ratio(per_variant, variant, name, base), mark))
            lines.append("| " + " | ".join(cells) + " |")
        lines.append("")
        lines.append("Summary against %s (geometric mean of the ratio over "
                     "the scenarios both have; scenarios slower or faster "
                     "by more than %d %%):" % (VARIANT_LABEL[base],
                                               int(PAR * 100)))
        lines.append("")
        lines.append("| variant | scenarios | geometric mean | slower | "
                     "faster |")
        lines.append("| --- | ---: | ---: | ---: | ---: |")
        for variant in variants:
            if variant == base:
                continue
            values = {n: ratio(per_variant, variant, n, base) for n in names}
            values = {n: v for n, v in values.items() if v is not None}
            if not values:
                continue
            slower = [n for n, v in values.items() if v > 1 + PAR]
            faster = [n for n, v in values.items() if v < 1 - PAR]
            lines.append("| %s | %d | %.3f | %d | %d |" % (
                VARIANT_LABEL[variant], len(values),
                geometric_mean(values.values()), len(slower), len(faster)))
        lines.append("")
        for variant in variants:
            if variant == base:
                continue
            values = {n: ratio(per_variant, variant, n, base) for n in names}
            slower = sorted(((v, n) for n, v in values.items()
                             if v is not None and v > 1 + PAR), reverse=True)
            if slower:
                lines.append("- %s is slower by more than %d %% in: %s" % (
                    VARIANT_LABEL[variant], int(PAR * 100),
                    ", ".join("%s %.2fx" % (n, v) for v, n in slower)))
        lines.append("")
        if LAYER_BASELINE in per_variant:
            lines.append("### %s: the layers of this library against %s"
                         % (toolchain, VARIANT_LABEL[LAYER_BASELINE]))
            lines.append("")
            layers = [v for v in ("lumex_cxx11", "lumex_cxx14", "lumex_cxx17",
                                  "lumex_cxx20", "lumex_cxx23")
                      if v in per_variant]
            lines.append("| scenario | " + " | ".join(
                VARIANT_LABEL[v] for v in layers) + " |")
            lines.append("| --- |" + " ---: |" * len(layers))
            for name in names:
                cells = []
                for variant in layers:
                    row = per_variant[variant].get(name)
                    if row is None:
                        cells.append("")
                    elif variant == LAYER_BASELINE:
                        cells.append(format_ns(row["median_ns"])
                                     + ("†" if unstable(row) else ""))
                    else:
                        cells.append("%s (%.2f)%s" % (
                            format_ns(row["median_ns"]),
                            ratio(per_variant, variant, name, LAYER_BASELINE),
                            "†" if unstable(row) else ""))
                lines.append("| %s | %s |" % (name, " | ".join(cells)))
            lines.append("")
            lines.append("| layer | geometric mean | slower | faster |")
            lines.append("| --- | ---: | ---: | ---: |")
            for variant in layers:
                if variant == LAYER_BASELINE:
                    continue
                values = [ratio(per_variant, variant, n, LAYER_BASELINE)
                          for n in names]
                values = [v for v in values if v is not None]
                lines.append("| %s | %.3f | %d | %d |" % (
                    VARIANT_LABEL[variant], geometric_mean(values),
                    len([v for v in values if v > 1 + PAR]),
                    len([v for v in values if v < 1 - PAR])))
            lines.append("")
    if size_rows:
        lines.extend(size_tables(size_rows))
    if compile_rows:
        lines.extend(compile_tables(compile_rows, compile_comments))
    return "\n".join(lines) + "\n"


def size_tables(size_rows):
    lines = ["## Sizes and properties", ""]
    lines.append("`sizeof` in bytes, then the properties that decide how the "
                 "type is passed and copied: TC trivially copyable (passed in "
                 "registers, copied with `memcpy`), TD trivially "
                 "destructible, SL standard layout, NM nothrow move "
                 "constructible. `array16_t` is `std::array<int, 16>`; "
                 "`text_error_t` is a struct that holds a `std::string`. "
                 "Outcome refuses a result whose value type and error type "
                 "are the same, so the pairs differ from `<T, T>`.")
    lines.append("")
    toolchains = []
    for row in size_rows:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
    for toolchain in toolchains:
        rows = [r for r in size_rows if r["toolchain"] == toolchain]
        variants = [v for v in VARIANT_ORDER
                    if any(r["variant"] == v for r in rows)]
        # one size/property column set per distinct implementation family
        seen = {}
        columns = []
        for variant in variants:
            sig = tuple((r["type"], r["sizeof"], r["trivially_copyable"],
                         r["trivially_destructible"], r["standard_layout"],
                         r["nothrow_move_constructible"])
                        for r in rows if r["variant"] == variant)
            if sig in seen:
                seen[sig].append(variant)
            else:
                seen[sig] = [variant]
                columns.append(sig)
        types = []
        for r in rows:
            if r["type"] not in types:
                types.append(r["type"])
        heads = []
        for sig in columns:
            names = seen[sig]
            if names[0].startswith("lumex") and len(names) > 1:
                heads.append("lumex (all of C++11 to C++23)"
                             if len(names) == 5 else
                             "lumex (" + ", ".join(
                                 VARIANT_LABEL[n].replace("lumex ", "")
                                 for n in names) + ")")
            else:
                heads.append(", ".join(VARIANT_LABEL[n] for n in names))
        lines.append("| `expected<T, E>` | " + " | ".join(heads) + " |")
        lines.append("| --- |" + " --- |" * len(columns))
        for type_name in types:
            cells = []
            for sig in columns:
                match = [s for s in sig if s[0] == type_name]
                if not match:
                    cells.append("")
                    continue
                _, size, tc, td, sl, nm = match[0]
                cells.append("%s B; TC %s TD %s SL %s NM %s" % (
                    size, "yes" if tc == "1" else "no",
                    "yes" if td == "1" else "no",
                    "yes" if sl == "1" else "no",
                    "yes" if nm == "1" else "no"))
            lines.append("| `%s` | %s |" % (type_name, " | ".join(cells)))
        lines.append("")
    return lines


def compile_tables(compile_rows, compile_comments):
    lines = ["## Compile time", ""]
    for comment in compile_comments:
        lines.append("- " + comment)
    lines.append("")
    lines.append("Wall time of `compile_time.cpp` (includes the header and "
                 "uses construction, copy, move, assignment, `value ()`, "
                 "`error ()`, `swap` and, where the type has them, "
                 "`value_or` and the monadic operations, for "
                 "`<int, unsigned>`, `<std::string, text_error>` and "
                 "`<void, int>`), median of the repeats, in milliseconds. "
                 "`syntax_O0` is `-fsyntax-only -O0`, `compile_O2` is "
                 "`-c -O2`. `no header` is the same unit without the "
                 "header under test: the cost of `<string>`, `<utility>` "
                 "and `<vector>`.")
    lines.append("")
    toolchains = []
    variants = []
    for row in compile_rows:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
        if row["variant"] not in variants:
            variants.append(row["variant"])
    order_all = ["none_cxx11", "none_cxx23"] + VARIANT_ORDER
    variants.sort(key=lambda v: order_all.index(v)
                  if v in order_all else len(order_all))
    for mode in ("syntax_O0", "compile_O2"):
        lines.append("| toolchain, %s | " % mode + " | ".join(
            VARIANT_LABEL[v] for v in variants) + " |")
        lines.append("| --- |" + " ---: |" * len(variants))
        for toolchain in toolchains:
            cells = []
            for variant in variants:
                match = [r for r in compile_rows
                         if r["toolchain"] == toolchain
                         and r["variant"] == variant and r["mode"] == mode]
                cells.append("%.0f" % float(match[0]["median_ms"])
                             if match else "")
            lines.append("| %s | %s |" % (toolchain, " | ".join(cells)))
        lines.append("")
    return lines


def svg_dot_chart(title, subtitle, per_variant, names, variants, base, low,
                  high, ticks, axis_text):
    left, top, row_h, width = 250, 70, 20, 980
    plot_w = width - left - 30
    height = top + row_h * len(names) + 78
    low_log, high_log = math.log(low), math.log(high)

    def x_of(value):
        clamped = min(max(math.log(max(value, 1e-9)), low_log), high_log)
        return left + (clamped - low_log) / (high_log - low_log) * plot_w

    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif" '
           'font-size="11">' % (width, height, width, height),
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="%d" y="22" font-size="15" font-weight="bold">%s</text>'
           % (left, escape(title)),
           '<text x="%d" y="40" fill="#555">%s</text>'
           % (left, escape(subtitle))]
    x = left
    for variant in [base] + variants:
        out.append('<circle cx="%d" cy="55" r="4.5" fill="%s"/>'
                   % (x + 5, VARIANT_COLOR[variant]))
        out.append('<text x="%d" y="59">%s</text>'
                   % (x + 14, escape(VARIANT_LABEL[variant])))
        x += 24 + 6.2 * len(VARIANT_LABEL[variant])
    for tick in ticks:
        gx = x_of(tick)
        stroke = "#444" if tick == 1.0 else "#ddd"
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%d" stroke="%s"/>'
                   % (gx, top - 6, gx, top + row_h * len(names), stroke))
        out.append('<text x="%.1f" y="%d" text-anchor="middle" fill="#555">'
                   '%s</text>' % (gx, top + row_h * len(names) + 14,
                                  "%gx" % tick))
    out.append('<text x="%d" y="%d" text-anchor="middle" fill="#555">%s'
               '</text>' % (left + plot_w // 2, top + row_h * len(names) + 34,
                            escape(axis_text)))
    out.append('<text x="%d" y="%d" text-anchor="middle" fill="#555">a '
               'hollow dot: the processes of that executable disagree by '
               'more than %d %%, read it as noise</text>'
               % (left + plot_w // 2, top + row_h * len(names) + 50,
                  int((UNSTABLE - 1) * 100)))
    for index, name in enumerate(names):
        y = top + index * row_h + row_h / 2
        if index % 2 == 0:
            out.append('<rect x="0" y="%.1f" width="%d" height="%d" '
                       'fill="#f6f8fa"/>' % (y - row_h / 2, width, row_h))
        out.append('<text x="%d" y="%.1f" text-anchor="end">%s</text>'
                   % (left - 8, y + 4, escape(name)))
        for variant in variants:
            row = per_variant[variant].get(name)
            if row is None:
                continue
            value = ratio(per_variant, variant, name, base)
            color = VARIANT_COLOR[variant]
            if unstable(row):
                fill = 'fill="none" stroke="%s" stroke-width="1.6"' % color
            else:
                fill = 'fill="%s" fill-opacity="0.9"' % color
            tip = "%s: %s ns, %.2fx%s" % (
                VARIANT_LABEL[variant], format_ns(row["median_ns"]), value,
                " (unstable)" if unstable(row) else "")
            out.append('<circle cx="%.1f" cy="%.1f" r="4" %s><title>%s'
                       '</title></circle>'
                       % (x_of(value), y, fill, escape(tip)))
    out.append("</svg>")
    return "\n".join(out) + "\n"


def svg_compile_chart(rows):
    toolchains = []
    variants = []
    for row in rows:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
        if row["variant"] not in variants:
            variants.append(row["variant"])
    order_all = ["none_cxx11", "none_cxx23"] + VARIANT_ORDER
    variants.sort(key=lambda v: order_all.index(v)
                  if v in order_all else len(order_all))
    modes = [("syntax_O0", "-fsyntax-only -O0"), ("compile_O2", "-c -O2")]
    maximum = max(float(r["median_ms"]) for r in rows)
    left, top, bar_h, gap, width = 140, 60, 14, 22, 900
    plot_w = width - left - 80
    blocks = len(toolchains) * len(modes)
    height = top + blocks * (len(variants) * bar_h + gap) + 30
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif" '
           'font-size="11">' % (width, height, width, height),
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="20" y="24" font-size="15" font-weight="bold">Compile '
           'time of one translation unit, ms</text>',
           '<text x="20" y="42" fill="#555">the unit includes the header '
           'and uses the common API; "no header" is the same unit without '
           'it</text>']
    y = top
    for toolchain in toolchains:
        for mode, mode_text in modes:
            out.append('<text x="20" y="%d" font-weight="bold">%s, %s</text>'
                       % (y + 12, escape(toolchain), escape(mode_text)))
            for variant in variants:
                match = [r for r in rows if r["toolchain"] == toolchain
                         and r["variant"] == variant and r["mode"] == mode]
                if not match:
                    continue
                value = float(match[0]["median_ms"])
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
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    csv_path, sizes_path = sys.argv[1], sys.argv[2]
    compile_path = sys.argv[3] if len(sys.argv) > 3 else None
    out_dir = os.path.dirname(os.path.abspath(csv_path))
    comments, rows = read_benchmark(csv_path)
    grouped, order = group_rows(rows)
    _, size_rows = read_comments_and_rows(sizes_path)
    compile_rows, compile_comments = [], []
    if compile_path and os.path.exists(compile_path):
        compile_comments, compile_rows = read_comments_and_rows(compile_path)
    with open(os.path.join(out_dir, "expected_benchmark.md"), "w") as handle:
        handle.write(markdown(comments, grouped, order, size_rows,
                              compile_rows, compile_comments))
    machine = next((c for c in comments if c.startswith("machine=")), "")
    model = machine.split('"')[1] if '"' in machine else machine
    for toolchain, per_variant in grouped.items():
        compilers = sorted({c.split("compiler=")[1].split(" library=")[0]
                            .strip('"') + " " + c.split("library=")[1]
                            .split(" cplusplus")[0].strip('"')
                            for c in comments
                            if c.startswith("variant %s " % toolchain)})
        subtitle = "%s; %s" % (", ".join(compilers), model)
        base = baseline_of(per_variant)
        names = ordered(order, per_variant, base)
        shown = [v for v in ("lumex_cxx23", "lumex_cxx11", "outcome_cxx17")
                 if v in per_variant and v != base]
        path = os.path.join(out_dir, "expected_benchmark_%s.svg" % toolchain)
        with open(path, "w") as handle:
            handle.write(svg_dot_chart(
                "expected, time relative to %s (%s)"
                % (VARIANT_LABEL[base], toolchain), subtitle, per_variant,
                names, shown, base, 0.25, 8.0,
                (0.25, 0.5, 1.0, 2.0, 4.0, 8.0),
                "time relative to %s (log scale, clipped at 0.25x and 8x; "
                "left is faster)" % VARIANT_LABEL[base]))
        print("wrote " + path)
        if LAYER_BASELINE in per_variant:
            layers = [v for v in ("lumex_cxx14", "lumex_cxx17", "lumex_cxx20",
                                  "lumex_cxx23") if v in per_variant]
            path = os.path.join(out_dir, "expected_layers_%s.svg" % toolchain)
            with open(path, "w") as handle:
                handle.write(svg_dot_chart(
                    "expected, the layers of this library relative to "
                    "lumex C++11 (%s)" % toolchain, subtitle, per_variant,
                    names, layers, LAYER_BASELINE, 0.5, 2.0,
                    (0.5, 0.7, 1.0, 1.4, 2.0),
                    "time relative to lumex C++11 (log scale, clipped at "
                    "0.5x and 2x; left is faster)"))
            print("wrote " + path)
    if compile_rows:
        path = os.path.join(out_dir, "compile_time.svg")
        with open(path, "w") as handle:
            handle.write(svg_compile_chart(compile_rows))
        print("wrote " + path)
    print("wrote " + os.path.join(out_dir, "expected_benchmark.md"))


if __name__ == "__main__":
    main()
