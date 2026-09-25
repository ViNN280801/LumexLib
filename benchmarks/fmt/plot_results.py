#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the CSV written by LumexFmtBenchmark into SVG charts and a table.

Standard library only (no matplotlib). The charts are vector SVG with a
viewBox, so they stay sharp at any zoom, in a browser, an IDE, the
repository web view and the Doxygen documentation.

Usage:
    python plot_results.py results/format_benchmark.csv

Writes next to the CSV:
    format_benchmark.svg           median ns per call (log scale), with the
                                   min..max range of the repetitions
    format_benchmark_relative.svg  time relative to LumexFormat (1.0 = same)
    format_benchmark.md            the numbers as a Markdown table
"""

import csv
import html
import math
import os
import sys

# Colour-blind safe palette (Okabe-Ito), fixed per method.
COLORS = {
    "LumexFormat": "#0072B2",
    "std::format": "#E69F00",
    "ostringstream": "#999999",
    "snprintf": "#009E73",
    "to_string": "#CC79A7",
}
FALLBACK_COLORS = ["#D55E00", "#56B4E9", "#F0E442"]

FONT = ("system-ui, -apple-system, 'Segoe UI', Roboto, 'Helvetica Neue', "
        "Arial, sans-serif")
CHAR_WIDTH = 6.6  # average glyph width at 12 px, for layout estimates


def read_results(path):
    """Returns (header comment, ordered scenarios, rows by scenario)."""
    header = ""
    lines = []
    with open(path, newline="", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#"):
                header = line[1:].strip()
            else:
                lines.append(line)
    scenarios = []
    by_scenario = {}
    for row in csv.DictReader(lines):
        for key in ("median_ns", "min_ns", "max_ns"):
            row[key] = float(row[key])
        if row["scenario"] not in by_scenario:
            scenarios.append(row["scenario"])
            by_scenario[row["scenario"]] = []
        by_scenario[row["scenario"]].append(row)
    return header, scenarios, by_scenario


def all_methods(scenarios, by_scenario):
    methods = []
    for scenario in scenarios:
        for row in by_scenario[scenario]:
            if row["method"] not in methods:
                methods.append(row["method"])
    return methods


def method_color(method, methods):
    if method in COLORS:
        return COLORS[method]
    return FALLBACK_COLORS[methods.index(method) % len(FALLBACK_COLORS)]


def nice_linear_ticks(maximum, count=6):
    """Round tick values from 0 to at least `maximum`."""
    raw = maximum / count
    magnitude = 10 ** math.floor(math.log10(raw))
    step = min((m * magnitude for m in (1, 2, 2.5, 5, 10)
                if m * magnitude >= raw), default=magnitude * 10)
    ticks = [0.0]
    while ticks[-1] < maximum:
        ticks.append(ticks[-1] + step)
    return ticks


def log_ticks(minimum, maximum):
    """1-2-5 ticks covering [minimum, maximum] on a log axis."""
    low = 10 ** math.floor(math.log10(minimum))
    ticks = []
    value = low
    while True:
        for factor in (1, 2, 5):
            tick = value * factor
            if tick >= low:
                ticks.append(tick)
            if tick >= maximum:
                return ticks
        value *= 10


def format_tick(value):
    if value >= 1000:
        return "%gk" % (value / 1000)
    return "%g" % value


def text(x, y, content, size=12, anchor="start", weight="normal",
         color="#1a202c", halo=False):
    """A text element; `halo` adds a white outline so it reads over lines."""
    outline = (' paint-order="stroke" stroke="#ffffff" stroke-width="3" '
               'stroke-linejoin="round"' if halo else '')
    return ('<text x="%.1f" y="%.1f" font-size="%d" text-anchor="%s" '
            'font-weight="%s" fill="%s"%s>%s</text>'
            % (x, y, size, anchor, weight, color, outline,
               html.escape(content)))


def bar_chart(title, subtitle, axis_label, scenarios, by_scenario, value_of,
              range_of=None, log_scale=False, reference_line=None,
              value_format="%.1f", unit=""):
    """Grouped horizontal bar chart as an SVG document string."""
    methods = all_methods(scenarios, by_scenario)
    bar = 16
    bar_gap = 4
    group_pad = 12
    label_width = 150
    plot_width = 620
    value_width = 110
    margin = 20
    width = margin + label_width + plot_width + value_width + margin

    # Legend rows, wrapped to the plot width.
    legend_rows = [[]]
    row_width = 0.0
    for method in methods:
        item = 18 + CHAR_WIDTH * len(method) + 22
        if row_width + item > label_width + plot_width and legend_rows[-1]:
            legend_rows.append([])
            row_width = 0.0
        legend_rows[-1].append(method)
        row_width += item
    top = 78 + 22 * len(legend_rows)

    groups = [(s, by_scenario[s]) for s in scenarios]
    plot_height = sum(len(rows) * (bar + bar_gap) - bar_gap + 2 * group_pad
                      for _, rows in groups)
    height = top + plot_height + 58

    highs = [range_of(row)[1] if range_of else value_of(row)
             for _, rows in groups for row in rows]
    if log_scale:
        lows = [range_of(row)[0] if range_of else value_of(row)
                for _, rows in groups for row in rows]
        ticks = log_ticks(min(lows), max(highs))
        low_log = math.log10(ticks[0])
        span = math.log10(ticks[-1]) - low_log

        def x_of(value):
            return plot_x + plot_width * (math.log10(max(value, ticks[0]))
                                          - low_log) / span
    else:
        limit = max(max(highs), reference_line or 0.0)
        ticks = nice_linear_ticks(limit)

        def x_of(value):
            return plot_x + plot_width * value / ticks[-1]

    plot_x = margin + label_width
    out = ['<?xml version="1.0" encoding="UTF-8"?>',
           '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" '
           'width="%d" height="%d" font-family="%s" '
           'text-rendering="geometricPrecision" '
           'shape-rendering="geometricPrecision">'
           % (width, height, width, height, html.escape(FONT, quote=True)),
           '<title>%s</title>' % html.escape(title),
           '<rect width="100%%" height="100%%" fill="#ffffff"/>',
           text(margin, 30, title, size=18, weight="600"),
           text(margin, 52, subtitle, size=12, color="#4a5568")]

    # Legend.
    y = 76
    for row_methods in legend_rows:
        x = float(margin)
        for method in row_methods:
            out.append('<rect x="%.1f" y="%.1f" width="12" height="12" rx="2" '
                       'fill="%s"/>'
                       % (x, y - 10, method_color(method, methods)))
            out.append(text(x + 18, y, method))
            x += 18 + CHAR_WIDTH * len(method) + 22
        y += 22

    # Scenario bands and labels.
    y = float(top)
    band_positions = []
    for index, (scenario, rows) in enumerate(groups):
        band_height = len(rows) * (bar + bar_gap) - bar_gap + 2 * group_pad
        if index % 2 == 0:
            out.append('<rect x="%d" y="%.1f" width="%d" height="%.1f" '
                       'fill="#f7fafc"/>'
                       % (margin, y, label_width + plot_width + value_width,
                          band_height))
        out.append(text(margin + 4, y + band_height / 2 + 4, scenario,
                        weight="600"))
        band_positions.append((y, rows))
        y += band_height
    plot_bottom = y

    # Grid and tick labels.
    for tick in ticks:
        x = x_of(tick)
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%.1f" '
                   'stroke="#e2e8f0" stroke-width="1"/>'
                   % (x, top, x, plot_bottom))
        out.append(text(x, plot_bottom + 18, format_tick(tick), size=11,
                        anchor="middle", color="#4a5568"))
    out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%.1f" stroke="#a0aec0" '
               'stroke-width="1"/>' % (plot_x, top, plot_x, plot_bottom))
    out.append(text(plot_x + plot_width / 2, plot_bottom + 40, axis_label,
                    size=12, anchor="middle", color="#2d3748"))

    # The reference line goes under the bars and their labels.
    if reference_line is not None:
        x = x_of(reference_line)
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%.1f" '
                   'stroke="#C53030" stroke-width="1.5" '
                   'stroke-dasharray="6 4"/>' % (x, top - 4, x, plot_bottom))

    # Bars, ranges and value labels.
    for band_top, rows in band_positions:
        y = band_top + group_pad
        for row in rows:
            value = value_of(row)
            x_end = x_of(value)
            color = method_color(row["method"], methods)
            tooltip = "%s / %s: %s%s" % (row["scenario"], row["method"],
                                         value_format % value, unit)
            out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%d" '
                       'rx="2" fill="%s"><title>%s</title></rect>'
                       % (plot_x, y, max(1.0, x_end - plot_x), bar, color,
                          html.escape(tooltip)))
            label_x = x_end + 6
            if range_of:
                low, high = range_of(row)
                x_low, x_high = x_of(low), x_of(high)
                middle = y + bar / 2
                out.append('<path d="M%.1f %.1fH%.1fM%.1f %.1fV%.1f'
                           'M%.1f %.1fV%.1f" stroke="#1a202c" '
                           'stroke-width="1.2" fill="none"/>'
                           % (x_low, middle, x_high, x_low, middle - 4,
                              middle + 4, x_high, middle - 4, middle + 4))
                label_x = max(label_x, x_high + 6)
            out.append(text(label_x, y + bar - 4, value_format % value + unit,
                            size=11, color="#2d3748", halo=True))
            y += bar + bar_gap

    out.append("</svg>")
    return "\n".join(out) + "\n"


def relative_to_lumex(by_scenario):
    """Adds `relative` = method time / LumexFormat time for each row."""
    for rows in by_scenario.values():
        base = next((r["median_ns"] for r in rows
                     if r["method"] == "LumexFormat"), None)
        for row in rows:
            row["relative"] = row["median_ns"] / base if base else 0.0


def markdown_table(header, scenarios, by_scenario):
    lines = ["# LumexFormat benchmark results", "",
             "Environment: `%s`." % header, "",
             "Median ns per call over the repetitions (lower is better); "
             "`x LumexFormat` is the time relative to LumexFormat "
             "(above 1.0 = slower than LumexFormat).", "",
             "| Scenario | Method | Median, ns | Min, ns | Max, ns "
             "| x LumexFormat |",
             "| --- | --- | ---: | ---: | ---: | ---: |"]
    for scenario in scenarios:
        for row in by_scenario[scenario]:
            lines.append("| `%s` | %s | %.1f | %.1f | %.1f | %.2f |"
                         % (scenario, row["method"], row["median_ns"],
                            row["min_ns"], row["max_ns"], row["relative"]))
    lines += ["", "Scenarios:", ""]
    for scenario in scenarios:
        lines.append("- `%s`: %s"
                     % (scenario, by_scenario[scenario][0]["description"]))
    lines.append("")
    return "\n".join(lines)


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    path = argv[1]
    header, scenarios, by_scenario = read_results(path)
    relative_to_lumex(by_scenario)
    stem = os.path.splitext(path)[0]

    with open(stem + ".svg", "w", encoding="utf-8", newline="\n") as stream:
        stream.write(bar_chart(
            "LumexFormat benchmark: time per call",
            header + "   |   median of the repetitions, whiskers = min..max",
            "nanoseconds per call (log scale, lower is better)",
            scenarios, by_scenario, lambda row: row["median_ns"],
            range_of=lambda row: (row["min_ns"], row["max_ns"]),
            log_scale=True, unit=" ns"))
    with open(stem + "_relative.svg", "w", encoding="utf-8",
              newline="\n") as stream:
        stream.write(bar_chart(
            "Time relative to LumexFormat",
            header + "   |   1.0 = LumexFormat (dashed line)",
            "time / LumexFormat time (lower is faster)",
            scenarios, by_scenario, lambda row: row["relative"],
            reference_line=1.0, value_format="%.2f", unit="x"))
    with open(stem + ".md", "w", encoding="utf-8", newline="\n") as stream:
        stream.write(markdown_table(header, scenarios, by_scenario))
    print("written %s.svg, %s_relative.svg, %s.md" % (stem, stem, stem))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
