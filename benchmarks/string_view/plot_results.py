#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the CSV of run_benchmark.py into a Markdown table and SVG charts.

    python plot_results.py results/string_view_benchmark.csv [results/compile_time.csv]

Reads string_view_traits.csv and library.csv from the directory of the CSV
when they exist. Writes next to the CSV:
    string_view_benchmark.md             the numbers: per build tree a table
                                         of the median time per operation of
                                         every variant and its ratio to
                                         std::string_view at C++17, summaries,
                                         the layout and traits of the views,
                                         the compiled library, and the
                                         compile times when given
    string_view_benchmark_<tree>.svg     one chart per build tree: a dot per
                                         variant and scenario at its time
                                         relative to std::string_view C++17
                                         (log scale)
    compile_time.svg                     the compile times, when given

starts_with and ends_with are C++20 in the standard library: for those
scenarios the baseline is std::string_view at C++20.

Standard library only.
"""

import csv
import math
import os
import sys
from xml.sax.saxutils import escape

VARIANT_ORDER = ["std_cxx17", "std_cxx20", "lumex_cxx11", "lumex_cxx17",
                 "lumex_unity_cxx17",
                 "boost_cxx11", "boost_cxx17"]
VARIANT_LABEL = {
    "std_cxx17": "std C++17",
    "std_cxx20": "std C++20",
    "lumex_cxx11": "lumex C++11",
    "lumex_cxx17": "lumex C++17",
    "lumex_unity_cxx17": "lumex inlined C++17",
    "boost_cxx11": "boost C++11",
    "boost_cxx17": "boost C++17",
    "none_cxx11": "no view C++11",
    "none_cxx17": "no view C++17",
    "none_cxx20": "no view C++20",
}
VARIANT_COLOR = {
    "std_cxx17": "#d9822b",
    "std_cxx20": "#b0601a",
    "lumex_cxx11": "#1b6ca8",
    "lumex_cxx17": "#6fb1e0",
    "lumex_unity_cxx17": "#7b4fb0",
    "boost_cxx11": "#3c9d4a",
    "boost_cxx17": "#8fd19a",
    "none_cxx11": "#888888",
    "none_cxx17": "#bbbbbb",
    "none_cxx20": "#d4d4d4",
}
BASELINE = "std_cxx17"
FALLBACK_BASELINE = "std_cxx20"
# A ratio within this band of 1.0 is called "on par" in the summary.
PAR = 0.10
# A scenario whose slowest pass is this much slower than its fastest pass is
# marked as unstable between processes.
UNSTABLE = 1.25
# Pairs of variants compared with each other in the second summary. A scenario
# that only exists for C++20 of the standard view (starts_with, ends_with) is
# compared with std_cxx20 through the fallback below.
PAIRS = [("lumex_cxx17", "std_cxx17"), ("lumex_cxx11", "std_cxx17"),
         ("lumex_cxx11", "lumex_cxx17"), ("lumex_cxx17", "boost_cxx17"),
         ("lumex_cxx11", "boost_cxx11"), ("boost_cxx17", "std_cxx17"),
         ("lumex_unity_cxx17", "std_cxx17"),
         ("lumex_cxx17", "lumex_unity_cxx17")]
COMPILE_MODES = [("include", "the header is included and one object is "
                  "declared (`-fsyntax-only -O0`)"),
                 ("use", "the whole API is used: construction, observers, "
                  "searches, comparisons, slicing, `std::sort`, "
                  "`std::map`, hashing (`-fsyntax-only -O0`)"),
                 ("use_o2", "the same use, compiled (`-c -O2`); the last "
                  "column group is the `.text` of the object in bytes")]


def read_csv_with_comments(path):
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
    comments, rows = read_csv_with_comments(path)
    for row in rows:
        row["median_ns"] = float(row["median_ns"])
        row["worst_median_ns"] = float(row.get("worst_median_ns")
                                       or row["median_ns"])
        row["min_ns"] = float(row["min_ns"])
        row["max_ns"] = float(row["max_ns"])
        row["size"] = int(row["size"])
    return comments, rows


def read_compile(path):
    comments, rows = read_csv_with_comments(path)
    for row in rows:
        row["median_ms"] = float(row["median_ms"])
        row["min_ms"] = float(row["min_ms"])
    return comments, rows


def read_plain(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def group(rows):
    """{toolchain: {variant: {scenario: row}}} and the scenario order."""
    result = {}
    order = []
    for row in rows:
        key = row["scenario"]
        if key not in order:
            order.append(key)
        result.setdefault(row["toolchain"], {}).setdefault(
            row["variant"], {})[key] = row
    return result, order


def variants_of(per_variant):
    return [v for v in VARIANT_ORDER if v in per_variant]


def baseline_row(per_variant, key):
    for name in (BASELINE, FALLBACK_BASELINE):
        if name in per_variant and key in per_variant[name]:
            return per_variant[name][key]
    return None


def pair_ratios(per_variant, numerator, denominator, order):
    """{scenario: time of `numerator` / time of `denominator`}; when the
    denominator is std_cxx17 and lacks the scenario, std_cxx20 stands in."""
    out = {}
    for key in order:
        top = per_variant[numerator].get(key)
        bottom = per_variant[denominator].get(key)
        if bottom is None and denominator == BASELINE:
            bottom = per_variant.get(FALLBACK_BASELINE, {}).get(key)
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


def yes(value):
    return "Y" if value == "1" else "n"


def markdown(comments, grouped, order, compile_rows, compile_comments,
             traits, libs):
    lines = ["# string_view benchmark results", ""]
    for comment in comments:
        lines.append("- " + comment)
    lines.append("")
    lines.append("Time per operation in nanoseconds: the lowest median of "
                 "15 repetitions over all passes (the best pass), and in "
                 "parentheses the ratio to `std::string_view` at C++17 (below "
                 "1.00 is faster than that; for `starts_with` and "
                 "`ends_with`, which the standard has from C++20, the ratio "
                 "is to `std::string_view` at C++20). The unit of an "
                 "operation is the description of the scenario. A dagger "
                 "marks a value whose slowest pass was more than %d %% "
                 "slower than the best one: the processes of that executable "
                 "do not agree (code and stack placement), so differences "
                 "inside that band are noise." % int((UNSTABLE - 1) * 100))
    lines.append("")
    for toolchain, per_variant in grouped.items():
        variants = variants_of(per_variant)
        lines.append("## Build tree: %s" % toolchain)
        lines.append("")
        header = ["scenario", "operation"] + [VARIANT_LABEL[v]
                                              for v in variants]
        lines.append("| " + " | ".join(header) + " |")
        lines.append("|" + "|".join([" --- "] * 2 + [" ---: "] * len(variants))
                     + "|")
        for key in order:
            base_row = baseline_row(per_variant, key)
            if base_row is None:
                continue
            base = base_row["median_ns"]
            description = next(r["description"] for v in variants
                               for r in [per_variant[v].get(key)] if r)
            cells = [key, description]
            for variant in variants:
                row = per_variant[variant].get(key)
                mark = "†" if row is not None and unstable(row) else ""
                if row is None:
                    cells.append("")
                elif row is base_row:
                    cells.append(format_ns(row["median_ns"]) + mark)
                else:
                    cells.append("%s (%.2f)%s" % (
                        format_ns(row["median_ns"]),
                        row["median_ns"] / max(base, 1e-9), mark))
            lines.append("| " + " | ".join(cells) + " |")
        lines.append("")
        lines.append("Variants compared with each other (time of the first "
                     "divided by the time of the second; the first is faster "
                     "below 1.00): geometric mean over the scenarios both "
                     "have, the best and the worst scenario of the first, and "
                     "the number of scenarios where the first is slower or "
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
                             len(faster), best[0], best[1], worst[0],
                             worst[1]))
        lines.append("")
        for numerator, denominator in PAIRS[:1] + PAIRS[5:7]:
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
                ", ".join("%s %.2fx" % (k, v) for v, k in slower)
                or "no scenario"))
            lines.append("- %s is faster than %s by more than %d %% in: %s" % (
                VARIANT_LABEL[numerator], VARIANT_LABEL[denominator],
                int(PAR * 100),
                ", ".join("%s %.2fx" % (k, v) for v, k in faster)
                or "no scenario"))
        lines.append("")
    if traits:
        lines.append("## Layout and traits")
        lines.append("")
        lines.append("`sizeof` in bytes and the properties of the view type. "
                     "TC: trivially copyable, TD: trivially destructible, "
                     "SL: standard layout, NC: nothrow copy constructible, "
                     "S: implicit from `std::string const &`, C: implicit "
                     "from `char const *`, STR: explicit conversion to "
                     "`std::string` possible (`std::string (v)`), IMP: "
                     "implicit conversion to `std::string`, TO/FROM: "
                     "implicit conversion to/from `std::string_view` (the "
                     "C++17 builds). The values do not depend on the build "
                     "tree.")
        lines.append("")
        toolchain = traits[0]["toolchain"]
        lines.append("| view | variant | sizeof | TC | TD | SL | NC | S | C | "
                     "STR | IMP | TO | FROM |")
        lines.append("| --- | --- | ---: | --- | --- | --- | --- | --- | "
                     "--- | --- | --- | --- | --- |")
        for row in traits:
            if row["toolchain"] != toolchain:
                continue
            lines.append("| `%s` | %s | %s | %s | %s | %s | %s | %s | %s | "
                         "%s | %s | %s | %s |" % (
                             row["type"], VARIANT_LABEL[row["variant"]],
                             row["sizeof"],
                             yes(row["trivially_copyable"]),
                             yes(row["trivially_destructible"]),
                             yes(row["standard_layout"]),
                             yes(row["nothrow_copy_constructible"]),
                             yes(row["from_std_string_implicit"]),
                             yes(row["from_cstr_implicit"]),
                             yes(row["to_std_string_constructible"]),
                             yes(row["to_std_string_implicit"]),
                             yes(row["to_std_string_view_implicit"]),
                             yes(row["from_std_string_view_implicit"])))
        lines.append("")
    if libs:
        lines.append("## The compiled library")
        lines.append("")
        lines.append("`libLumexCore_string_view` of each build tree (the "
                     "narrow and the wide view): file size, size after "
                     "`strip --strip-unneeded`, the `.text` of its code, and "
                     "the defined exported function symbols (`nm`).")
        lines.append("")
        lines.append("| build tree | kind | file | bytes | stripped | .text | "
                     "function symbols | narrow | wide |")
        lines.append("| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | "
                     "---: |")
        for row in libs:
            lines.append("| %s | %s | %s | %s | %s | %s | %s | %s | %s |" % (
                row["toolchain"], row["kind"], row["file"], row["bytes"],
                row["stripped_bytes"], row["text_bytes"],
                row["defined_text_symbols"], row["narrow_symbols"],
                row["wide_symbols"]))
        lines.append("")
    if compile_rows:
        lines.append("## Compile time")
        lines.append("")
        for comment in compile_comments:
            lines.append("- " + comment)
        lines.append("")
        lines.append("Wall time of `compile_time.cpp`, median of the repeats, "
                     "in milliseconds. `no view` is the same unit with a "
                     "stub instead of a view: the cost of the shared headers "
                     "(`<string>`, `<vector>`, `<map>`, `<algorithm>`). The "
                     "unit of `std C++17` leaves `starts_with` and "
                     "`ends_with` out (they are C++20), so it does a little "
                     "less than the others.")
        lines.append("")
        toolchains = []
        for row in compile_rows:
            if row["toolchain"] not in toolchains:
                toolchains.append(row["toolchain"])
        variants = compile_variants(compile_rows)
        for mode, text in COMPILE_MODES:
            lines.append("Unit `%s`: %s." % (mode, text))
            lines.append("")
            extra = mode == "use_o2"
            lines.append("| toolchain | " + " | ".join(
                VARIANT_LABEL[v] for v in variants)
                + ((" | " + " | ".join("%s .text" % VARIANT_LABEL[v]
                                       for v in variants)) if extra else "")
                + " |")
            lines.append("| --- |" + " ---: |" * (len(variants)
                                                  * (2 if extra else 1)))
            for toolchain in toolchains:
                cells = []
                sizes = []
                for variant in variants:
                    match = [r for r in compile_rows
                             if r["toolchain"] == toolchain
                             and r["variant"] == variant
                             and r["mode"] == mode]
                    cells.append("%.0f" % match[0]["median_ms"] if match
                                 else "")
                    sizes.append(match[0].get("text_bytes", "") if match
                                 else "")
                lines.append("| %s | %s |" % (toolchain, " | ".join(
                    cells + (sizes if extra else []))))
            lines.append("")
    return "\n".join(lines) + "\n"


def compile_variants(rows):
    variants = []
    for row in rows:
        if row["variant"] not in variants:
            variants.append(row["variant"])
    order_all = ["none_cxx11", "none_cxx17", "none_cxx20"] + VARIANT_ORDER
    variants.sort(key=lambda v: order_all.index(v)
                  if v in order_all else len(order_all))
    return variants


def svg_dot_chart(title, subtitle, per_variant, order):
    variants = variants_of(per_variant)
    keys = [k for k in order if baseline_row(per_variant, k) is not None]
    left, top, row_h, width = 190, 70, 18, 1000
    plot_w = width - left - 30
    height = top + row_h * len(keys) + 78
    low, high = math.log(0.05), math.log(50.0)

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
    for ratio in (0.05, 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 50.0):
        gx = x_of(ratio)
        stroke = "#444" if ratio == 1.0 else "#ddd"
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%d" stroke="%s"/>'
                   % (gx, top - 6, gx, top + row_h * len(keys), stroke))
        out.append('<text x="%.1f" y="%d" text-anchor="middle" fill="#555">'
                   '%s</text>' % (gx, top + row_h * len(keys) + 14,
                                  ("%gx" % ratio)))
    out.append('<text x="%d" y="%d" text-anchor="middle" fill="#555">time '
               'relative to std::string_view C++17 (C++20 for starts_with '
               'and ends_with); log scale, clipped at 0.05x and 50x; left '
               'is faster</text>'
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
                   % (left - 8, y + 4, escape(key)))
        base = baseline_row(per_variant, key)["median_ns"]
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


def svg_compile_chart(rows):
    toolchains = []
    for row in rows:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
    variants = compile_variants(rows)
    modes = [m for m, _ in COMPILE_MODES if any(r["mode"] == m for r in rows)]
    maximum = max(r["median_ms"] for r in rows)
    left, top, bar_h, gap, width = 130, 60, 14, 22, 900
    plot_w = width - left - 80
    height = (top + len(toolchains) * len(modes)
              * (len(variants) * bar_h + gap) + 30)
    out = ['<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
           'viewBox="0 0 %d %d" font-family="Segoe UI, Arial, sans-serif" '
           'font-size="11">' % (width, height, width, height),
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="20" y="24" font-size="15" font-weight="bold">Compile '
           'time of one translation unit, ms</text>',
           '<text x="20" y="42" fill="#555">include: the header and one '
           'object (-fsyntax-only -O0); use: the whole API (-fsyntax-only '
           '-O0); use_o2: the same, compiled (-c -O2); "no view" is the same '
           'unit with a stub</text>']
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
    traits_path = os.path.join(out_dir, "string_view_traits.csv")
    traits = read_plain(traits_path) if os.path.exists(traits_path) else []
    lib_path = os.path.join(out_dir, "library.csv")
    libs = read_plain(lib_path) if os.path.exists(lib_path) else []
    with open(os.path.join(out_dir, "string_view_benchmark.md"), "w") as out:
        out.write(markdown(comments, grouped, order, compile_rows,
                           compile_comments, traits, libs))
    machine = next((c for c in comments if c.startswith("machine=")), "")
    for toolchain, per_variant in grouped.items():
        compilers = sorted({c.split("compiler=")[1].split(" library=")[0]
                            .strip('"')
                            for c in comments
                            if c.startswith("variant %s " % toolchain)})
        model = machine.split('"')[1] if '"' in machine else machine
        subtitle = "%s; %s" % (", ".join(compilers), model)
        path = os.path.join(out_dir,
                            "string_view_benchmark_%s.svg" % toolchain)
        with open(path, "w") as handle:
            handle.write(svg_dot_chart(
                "string_view, time relative to std::string_view C++17 "
                "(library %s)" % toolchain, subtitle, per_variant, order))
        print("wrote " + path)
    if compile_rows:
        path = os.path.join(out_dir, "compile_time.svg")
        with open(path, "w") as handle:
            handle.write(svg_compile_chart(compile_rows))
        print("wrote " + path)
    print("wrote " + os.path.join(out_dir, "string_view_benchmark.md"))


if __name__ == "__main__":
    main()
