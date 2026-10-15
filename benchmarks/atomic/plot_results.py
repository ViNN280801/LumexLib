#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the results of run_benchmark.py into SVG charts and a table.

Standard library only (no matplotlib). The charts are vector SVG with a
viewBox, so they stay sharp at any zoom, in a browser, an IDE, the
repository web view and the Doxygen documentation.

Usage:
    python plot_results.py [--prefix NAME_] results/atomic_benchmark.csv
                           [more.csv ...]

More files (for example a run on another machine or compiler) add their
series to the same charts; a ratio to the baseline of its own process is
what makes series from different processes and machines comparable.

Writes next to the first CSV (--prefix puts NAME_ in front of the name of
every SVG file, not of the Markdown file: the charts of results/msvc/ are
named msvc_atomic_benchmark_*.svg so that an image name is unique in the
documentation):
    atomic_benchmark_baseline.svg      the uint64 CAS baseline before each
                                       implementation (should overlap)
    atomic_benchmark_<operation>.svg   load, store, exchange, load_one_writer and
                                       compare_exchange_strong under
                                       contention, as ratios to the baseline
    atomic_benchmark_uncontended.svg   one thread, nanoseconds
    atomic_compile_time.svg            the compile times of one translation
                                       unit per implementation, when
                                       compile_time.csv (written by
                                       run_benchmark.py --compile-time-only)
                                       is next to the first CSV
    atomic_benchmark.md                the numbers as Markdown tables, and
                                       the compile times when given
A results file of the scenario `store_wait` (the time of every single store
of one writer under a read load, in ns) gets the charts
<stem>_{median,p99,p9999,max}.svg on a logarithmic axis and the tables in
<stem>.md instead.
"""

import csv
import html
import math
import os
import sys

OPERATIONS = ("load", "store", "exchange", "compare_exchange_strong",
              "load_one_writer", "one_writer_stores", "load_store",
              "weak_load", "weak_load_lock")
# The rows of the destruction-latency scenario (mode `destroy`).
DESTROY_OPERATIONS = ("destroy_median", "destroy_p99", "destroy_max",
                      "destroy_backlog")
# The rows of the store-wait scenario (mode `store_wait`): the time of every
# single store of one writer under a read load, in ns.
STORE_WAIT_OPERATIONS = ("store_wait_timer", "store_wait_median",
                         "store_wait_p90", "store_wait_p99",
                         "store_wait_p999", "store_wait_p9999",
                         "store_wait_max", "store_wait_over_1ms",
                         "store_wait_switches",
                         "store_wait_over_1ms_switched")
COLORS = {
    "lumex_lock_based_cxx11": "#0072B2",
    "lumex_lock_based": "#56B4E9",
    "lumex_lock_free_cxx11": "#CC79A7",
    "lumex_lock_free": "#D55E00",
    "lumex_lock_free_deferred": "#F0E442",
    "lumex_default": "#009E73",
    "lumex_std_backed": "#999999",
    "std": "#E69F00",
    "boost": "#000000",
    "lumex_split_count_cxx11": "#332288",
    "lumex_split_count": "#882255",
    "lumex_split_count_r4": "#AA4499",
    "lumex_lock_based_lumex_cxx11": "#117733",
    "lumex_lock_based_lumex": "#44AA99",
    "lumex_split_count_weak": "#882255",
    "lumex_lock_based_lumex_weak": "#44AA99",
    "lumex_lock_based_weak": "#0072B2",
    "lumex_lock_free_weak": "#D55E00",
    "lumex_std_backed_weak": "#999999",
    "std_weak": "#E69F00",
}
FALLBACK_COLORS = ["#8c564b", "#7f7f7f", "#17becf", "#bcbd22"]
# The variants of compile_time.cpp (run_benchmark.py --compile-time-only):
# (name, label, color), in the order of the chart.
COMPILE_VARIANTS = [
    ("none_cxx11", "stub C++11", "#707070"),
    ("none_cxx20", "stub C++20", "#B0B0B0"),
    ("lumex_cxx11", "LumexLib common name, C++11", "#005F46"),
    ("lumex_cxx20", "LumexLib common name, C++20", "#009E73"),
    ("lumex_lock_based_cxx11", "LumexLib lock-based, C++11", "#0072B2"),
    ("lumex_lock_based_cxx20", "LumexLib lock-based, C++20", "#56B4E9"),
    ("lumex_lock_free_cxx11", "LumexLib lock-free, C++11", "#CC79A7"),
    ("lumex_lock_free_cxx20", "LumexLib lock-free, C++20", "#D55E00"),
    ("lumex_std_backed_cxx20", "LumexLib std-backed, C++20", "#999999"),
    ("std_cxx20", "std::atomic<std::shared_ptr>, C++20", "#E69F00"),
    ("boost_cxx11", "boost::atomic_shared_ptr, C++11", "#4D4D4D"),
    ("boost_cxx20", "boost::atomic_shared_ptr, C++20", "#000000"),
]
COMPILE_MODES = [("include", "the header is included and one object is "
                  "declared"),
                 ("use", "the operations the benchmark measures are used: "
                  "`load`, `store`, `exchange`, `compare_exchange_strong` "
                  "and `is_lock_free`")]
DASHES = ["", "7 4", "2 4", "", "9 3 2 3", "4 4"]
FONT = ("system-ui, -apple-system, 'Segoe UI', Roboto, 'Helvetica Neue', "
        "Arial, sans-serif")
CHAR_WIDTH = 6.6  # average glyph width at 12 px, for layout estimates


def read_results(path):
    """Returns (metadata dict, implementations, rows)."""
    meta = {}
    implementations = []
    lines = []
    with open(path, newline="", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#"):
                key, _, value = line[1:].strip().partition("=")
                if key == "implementation":
                    fields = value.split("|")
                    implementations.append({
                        "key": fields[0], "label": fields[1],
                        "path": fields[2], "standard": fields[3],
                        "lock_free": fields[4]})
                else:
                    meta[key] = value
            else:
                lines.append(line)
    rows = []
    for row in csv.DictReader(lines):
        row["threads"] = int(row["threads"])
        for key in ("ratio_median", "ratio_p25", "ratio_p75", "ns_median",
                    "ns_p25", "ns_p75", "ns_min", "ns_max", "success_rate",
                    "cycles_median", "instructions_median"):
            if key in row:
                row[key] = float(row[key])
        for key in ("operations_sum", "successes_sum"):
            if key in row:
                row[key] = int(row[key])
        rows.append(row)
    return meta, implementations, rows


class Series:
    """One implementation of one results file."""

    def __init__(self, index, source, implementation, meta, several):
        self.index = index
        self.source = source
        self.key = implementation["key"]
        self.implementation = implementation
        self.label = implementation["label"]
        if several:
            self.label += " [%s, %s]" % (meta.get("compiler", "?"),
                                         meta.get("cpu", "?"))
        self.color = COLORS.get(self.key) if source == 0 else None
        if self.color is None:
            self.color = FALLBACK_COLORS[index % len(FALLBACK_COLORS)]
        self.dash = DASHES[index % len(DASHES)]


def load_all(paths):
    """Returns (metadata of each file, series, rows keyed by series)."""
    metas = []
    series = []
    rows_of = {}
    for source, path in enumerate(paths):
        meta, implementations, rows = read_results(path)
        metas.append(meta)
        for implementation in implementations:
            item = Series(len(series), source, implementation, meta,
                          len(paths) > 1)
            series.append(item)
            rows_of[item] = [r for r in rows
                             if r["implementation"] == item.key]
    return metas, series, rows_of


def text(x, y, content, size=12, anchor="start", weight="normal",
         color="#1a202c"):
    """A text element."""
    return ('<text x="%.1f" y="%.1f" font-size="%d" text-anchor="%s" '
            'font-weight="%s" fill="%s">%s</text>'
            % (x, y, size, anchor, weight, color, html.escape(content)))


def nice_ticks(maximum, count=6):
    """Round tick values from 0 to at least `maximum`."""
    raw = max(maximum, 1e-9) / count
    magnitude = 10 ** math.floor(math.log10(raw))
    step = min((m * magnitude for m in (1, 2, 2.5, 5, 10)
                if m * magnitude >= raw), default=magnitude * 10)
    ticks = [0.0]
    while ticks[-1] < maximum:
        ticks.append(ticks[-1] + step)
    return ticks


def svg_open(width, height, title):
    return ['<?xml version="1.0" encoding="UTF-8"?>',
            '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 %d %d" '
            'width="%d" height="%d" font-family="%s" '
            'text-rendering="geometricPrecision" '
            'shape-rendering="geometricPrecision">'
            % (width, height, width, height, html.escape(FONT, quote=True)),
            '<title>%s</title>' % html.escape(title),
            '<rect width="100%" height="100%" fill="#ffffff"/>']


def legend(out, series, x0, y0, max_width):
    """Writes the legend; returns the y below it."""
    x = float(x0)
    y = float(y0)
    for item in series:
        width = 30 + CHAR_WIDTH * len(item.label) + 22
        if x + width > x0 + max_width and x > x0:
            x = float(x0)
            y += 22
        out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" '
                   'stroke="%s" stroke-width="2.5"%s/>'
                   % (x, y - 4, x + 24, y - 4, item.color,
                      ' stroke-dasharray="%s"' % item.dash
                      if item.dash else ''))
        out.append('<circle cx="%.1f" cy="%.1f" r="3.5" fill="%s"/>'
                   % (x + 12, y - 4, item.color))
        out.append(text(x + 30, y, item.label))
        x += width
    return y + 22


def line_chart(title, subtitle, y_label, lines, reference=None,
               reference_label="", log=False):
    """`lines`: [(Series, [(threads, value, low, high)])] -> SVG text.

    `subtitle` is a list of lines. With `log` the value axis is logarithmic
    (decades), for latencies that span orders of magnitude.
    """
    width = 900
    margin = 20
    plot_left = 80
    plot_width = width - plot_left - 30
    plot_height = 330
    out = svg_open(width, 0, title)
    out.append(text(margin, 30, title, size=18, weight="600"))
    for index, line in enumerate(subtitle):
        out.append(text(margin, 52 + 17 * index, line, size=12,
                        color="#4a5568"))
    top = legend(out, [s for s, _ in lines], margin,
                 78 + 17 * (len(subtitle) - 1), width - 2 * margin) + 6
    bottom = top + plot_height

    xs = sorted({p[0] for _, points in lines for p in points})
    highs = [p[3] for _, points in lines for p in points]
    if reference is not None:
        highs.append(reference)
    if log:
        values = [p[2] for _, points in lines for p in points if p[2] > 0]
        low_exp = math.floor(math.log10(min(values))) if values else 0
        high_exp = math.ceil(math.log10(max(highs))) if highs else 1
        if high_exp <= low_exp:
            high_exp = low_exp + 1
        ticks = [10.0 ** e for e in range(low_exp, high_exp + 1)]
    else:
        ticks = nice_ticks(max(highs) if highs else 1.0)
    x_min, x_max = (xs[0], xs[-1]) if xs else (0, 1)
    span = max(x_max - x_min, 1)

    def x_of(value):
        return plot_left + 20 + (plot_width - 40) * (value - x_min) / span

    def y_of(value):
        if log:
            return bottom - plot_height * (
                math.log10(max(value, ticks[0])) - low_exp) / (
                    high_exp - low_exp)
        return bottom - plot_height * value / ticks[-1]

    for tick in ticks:
        y = y_of(tick)
        out.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" '
                   'stroke="#e2e8f0" stroke-width="1"/>'
                   % (plot_left, y, plot_left + plot_width, y))
        out.append(text(plot_left - 8, y + 4,
                        "%d" % tick if log else "%g" % tick, size=11,
                        anchor="end", color="#4a5568"))
    for value in xs:
        x = x_of(value)
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%d" '
                   'stroke="#f0f3f7" stroke-width="1"/>'
                   % (x, top, x, bottom))
        out.append(text(x, bottom + 18, "%d" % value, size=11,
                        anchor="middle", color="#4a5568"))
    out.append('<line x1="%d" y1="%d" x2="%d" y2="%d" stroke="#a0aec0" '
               'stroke-width="1"/>' % (plot_left, bottom,
                                       plot_left + plot_width, bottom))
    out.append(text(plot_left + plot_width / 2, bottom + 40, "threads",
                    anchor="middle", color="#2d3748"))
    out.append('<text x="%d" y="%.1f" font-size="12" text-anchor="middle" '
               'fill="#2d3748" transform="rotate(-90 %d %.1f)">%s</text>'
               % (24, top + plot_height / 2, 24, top + plot_height / 2,
                  html.escape(y_label)))
    if reference is not None:
        y = y_of(reference)
        out.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" '
                   'stroke="#C53030" stroke-width="1.2" '
                   'stroke-dasharray="6 4"/>'
                   % (plot_left, y, plot_left + plot_width, y))
        out.append(text(plot_left + plot_width - 4, y - 6, reference_label,
                        size=11, anchor="end", color="#C53030"))

    count = len(lines)
    for position, (item, points) in enumerate(lines):
        if not points:
            continue
        offset = (position - (count - 1) / 2.0) * 3.0
        path = " ".join("%s%.1f %.1f" % ("M" if i == 0 else "L",
                                         x_of(p[0]) + offset, y_of(p[1]))
                        for i, p in enumerate(points))
        out.append('<path d="%s" fill="none" stroke="%s" stroke-width="2.2"'
                   '%s/>' % (path, item.color,
                             ' stroke-dasharray="%s"' % item.dash
                             if item.dash else ''))
        for threads, value, low, high in points:
            x = x_of(threads) + offset
            out.append('<path d="M%.1f %.1fV%.1fM%.1f %.1fH%.1fM%.1f %.1fH%.1f" '
                       'stroke="%s" stroke-width="1.1" fill="none"/>'
                       % (x, y_of(low), y_of(high), x - 3, y_of(low), x + 3,
                          x - 3, y_of(high), x + 3, item.color))
            out.append('<circle cx="%.1f" cy="%.1f" r="3.6" fill="%s" '
                       'stroke="#ffffff" stroke-width="1"><title>%s</title>'
                       '</circle>'
                       % (x, y_of(value), item.color,
                          html.escape("%s, %d threads: %.2f (%.2f..%.2f)"
                                      % (item.label, threads, value, low,
                                         high))))
    height = bottom + 60
    out[1] = out[1].replace('0 0 %d 0"' % width, '0 0 %d %d"'
                            % (width, height)).replace(
        'height="0"', 'height="%d"' % height)
    out.append("</svg>")
    return "\n".join(out) + "\n"


def bar_chart(title, subtitle, axis_label, groups):
    """`groups`: [(name, [(Series, value, low, high, note)])] -> SVG text.

    `subtitle` is a list of lines.
    """
    bar = 16
    bar_gap = 4
    group_pad = 12
    label_width = 190
    plot_width = 560
    value_width = 150
    margin = 20
    width = margin + label_width + plot_width + value_width + margin
    out = svg_open(width, 0, title)
    out.append(text(margin, 30, title, size=18, weight="600"))
    for index, line in enumerate(subtitle):
        out.append(text(margin, 52 + 17 * index, line, size=12,
                        color="#4a5568"))
    series = []
    for _, bars in groups:
        for item in bars:
            if item[0] not in series:
                series.append(item[0])
    top = legend(out, series, margin, 78 + 17 * (len(subtitle) - 1),
                 width - 2 * margin) + 6
    highs = [b[3] for _, bars in groups for b in bars]
    ticks = nice_ticks(max(highs) if highs else 1.0)
    plot_x = margin + label_width

    def x_of(value):
        return plot_x + plot_width * value / ticks[-1]

    y = float(top)
    bands = []
    for index, (name, bars) in enumerate(groups):
        band = len(bars) * (bar + bar_gap) - bar_gap + 2 * group_pad
        if index % 2 == 0:
            out.append('<rect x="%d" y="%.1f" width="%d" height="%.1f" '
                       'fill="#f7fafc"/>'
                       % (margin, y, label_width + plot_width + value_width,
                          band))
        out.append(text(margin + 4, y + band / 2 + 4, name, weight="600"))
        bands.append((y, bars))
        y += band
    plot_bottom = y
    for tick in ticks:
        x = x_of(tick)
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%.1f" '
                   'stroke="#e2e8f0" stroke-width="1"/>'
                   % (x, top, x, plot_bottom))
        out.append(text(x, plot_bottom + 18, "%g" % tick, size=11,
                        anchor="middle", color="#4a5568"))
    out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%.1f" stroke="#a0aec0" '
               'stroke-width="1"/>' % (plot_x, top, plot_x, plot_bottom))
    out.append(text(plot_x + plot_width / 2, plot_bottom + 40, axis_label,
                    anchor="middle", color="#2d3748"))
    for band_top, bars in bands:
        y = band_top + group_pad
        for item, value, low, high, note in bars:
            out.append('<rect x="%.1f" y="%.1f" width="%.1f" height="%d" '
                       'rx="2" fill="%s"><title>%s</title></rect>'
                       % (plot_x, y, max(1.0, x_of(value) - plot_x), bar,
                          item.color,
                          html.escape("%s: %s" % (item.label, note))))
            middle = y + bar / 2
            out.append('<path d="M%.1f %.1fH%.1fM%.1f %.1fV%.1fM%.1f %.1fV%.1f" '
                       'stroke="#1a202c" stroke-width="1.1" fill="none"/>'
                       % (x_of(low), middle, x_of(high), x_of(low),
                          middle - 4, middle + 4, x_of(high), middle - 4,
                          middle + 4))
            out.append(text(max(x_of(value), x_of(high)) + 6, y + bar - 4,
                            note, size=11, color="#2d3748"))
            y += bar + bar_gap
    height = int(plot_bottom + 58)
    out[1] = out[1].replace('0 0 %d 0"' % width, '0 0 %d %d"'
                            % (width, height)).replace(
        'height="0"', 'height="%d"' % height)
    out.append("</svg>")
    return "\n".join(out) + "\n"


class CompileSeries:
    """What bar_chart needs of a series: a label, a color and no dash."""

    def __init__(self, label, color):
        self.label = label
        self.color = color
        self.dash = ""


def read_compile(path):
    """Returns (comment lines, rows) of compile_time.csv."""
    comments = []
    lines = []
    with open(path, newline="", encoding="utf-8") as stream:
        for line in stream:
            if line.startswith("#"):
                comments.append(line[1:].strip())
            else:
                lines.append(line)
    rows = list(csv.DictReader(lines))
    for row in rows:
        row["median_ms"] = float(row["median_ms"])
    return comments, rows


def compile_chart(rows, comments):
    """A group per compiler and unit, a bar per variant the compiler built."""
    series = {name: CompileSeries(label, color)
              for name, label, color in COMPILE_VARIANTS}
    toolchains = []
    for row in rows:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
    groups = []
    for toolchain in toolchains:
        for mode, _ in COMPILE_MODES:
            bars = []
            for name, _, _ in COMPILE_VARIANTS:
                for row in rows:
                    if (row["toolchain"] == toolchain and row["mode"] == mode
                            and row["variant"] == name):
                        value = row["median_ms"]
                        bars.append((series[name], value, value, value,
                                     "%.0f ms" % value))
            if bars:
                groups.append(("%s: %s" % (toolchain, mode), bars))
    return bar_chart(
        "Compile time of one translation unit (-fsyntax-only -O0), ms",
        ["; ".join(c.replace("compiler ", "") for c in comments),
         "median of the repeats; the stub is a std::shared_ptr behind a "
         "std::mutex: the cost of the shared headers"],
        "milliseconds per translation unit (lower is faster)", groups)


def compile_markdown(rows, comments):
    lines = ["", "## Compile time", ""]
    for comment in comments:
        lines.append("- " + comment)
    lines.append("")
    lines.append("Wall time of `-fsyntax-only -O0` of `compile_time.cpp`, "
                 "median of the repeats, in milliseconds. The stub is a "
                 "`std::shared_ptr` behind a `std::mutex`: the cost of the "
                 "headers every variant shares. An empty cell is a variant "
                 "the compiler could not build (the standard library has no "
                 "`std::atomic<std::shared_ptr>`).")
    lines.append("")
    toolchains = []
    for row in rows:
        if row["toolchain"] not in toolchains:
            toolchains.append(row["toolchain"])
    shown = [v for v in COMPILE_VARIANTS
             if any(r["variant"] == v[0] for r in rows)]
    for mode, text_of_mode in COMPILE_MODES:
        lines.append("Unit `%s`: %s." % (mode, text_of_mode))
        lines.append("")
        lines.append("| toolchain | " + " | ".join(label for _, label, _
                                                  in shown) + " |")
        lines.append("| --- |" + " ---: |" * len(shown))
        for toolchain in toolchains:
            cells = []
            for name, _, _ in shown:
                match = [r for r in rows if r["toolchain"] == toolchain
                         and r["mode"] == mode and r["variant"] == name]
                cells.append("%.0f" % match[0]["median_ms"] if match else "")
            lines.append("| %s | %s |" % (toolchain, " | ".join(cells)))
        lines.append("")
    return lines


def present(series, rows_of):
    """The operations some series has rows for (older files lack newer ones)."""
    return tuple(op for op in OPERATIONS
                 if any(r["operation"] == op for s in series
                        for r in rows_of[s]))


def present_contended(series, rows_of):
    """The operations some series has contended rows for (load_store and
    the like are measured on one thread only)."""
    return tuple(op for op in present(series, rows_of)
                 if any(r["operation"] == op and r["mode"] == "contended"
                        for s in series for r in rows_of[s]))


def contended(rows, operation):
    return sorted((r for r in rows if r["mode"] == "contended"
                   and r["operation"] == operation),
                  key=lambda r: r["threads"])


def uncontended(rows, operation):
    return next((r for r in rows if r["mode"] == "uncontended"
                 and r["operation"] == operation), None)


def describe(meta):
    return ("%s, %s logical CPUs, governor %s; %s, %s; median of %s runs"
            % (meta.get("cpu", "?"), meta.get("logical_cpus", "?"),
               meta.get("governor", "?"), meta.get("compiler", "?"),
               meta.get("library", "?"), meta.get("repetitions", "?")))


def markdown(metas, series, rows_of):
    lines = ["# Atomic shared pointer benchmark results", ""]
    for index, item in enumerate(metas):
        lines.append("- Run %d: %s, %s logical CPUs, governor `%s`, kernel "
                     "`%s` (%s); %s with %s, %s build; %s repetitions, %s s "
                     "pauses, %s ms windows; measured %s in %s minutes, "
                     "load average %s at the start and %s at the end."
                     % (index + 1, item.get("cpu", "?"),
                        item.get("logical_cpus", "?"),
                        item.get("governor", "?"), item.get("kernel", "?"),
                        item.get("os", "?"), item.get("compiler", "?"),
                        item.get("library", "?"), item.get("build", "?"),
                        item.get("repetitions", "?"),
                        item.get("settle_seconds", "?"),
                        item.get("duration_ms", "?"), item.get("date", "?"),
                        item.get("minutes", "?"),
                        item.get("load_average_start", "?"),
                        item.get("load_average_end", "?")))
    lines += ["", "| Series | Selected implementation | Language mode "
              "| is_lock_free |", "| --- | --- | ---: | ---: |"]
    for item in series:
        lines.append("| %s | `%s` | %s | %s |"
                     % (item.label, item.implementation["path"],
                        item.implementation["standard"],
                        "yes" if item.implementation["lock_free"] == "1"
                        else "no"))
    threads = sorted({r["threads"] for s in series for r in rows_of[s]
                      if r["mode"] == "contended"})
    head = "| Series | " + " | ".join(str(t) for t in threads) + " |"
    rule = "| --- | " + " | ".join("---:" for _ in threads) + " |"

    lines += ["", "## Baseline", "",
              "`std::atomic<uint64_t>::compare_exchange_strong` with all "
              "threads on one word, timed right before each series' block "
              "(ns per operation and thread, median). It is the same code "
              "every time, so the rows should agree; the ratio of each row "
              "to the mean of the rows shows how far the machine drifted "
              "between the blocks.", "", head, rule]
    base = {s: {r["threads"]: r for r in contended(rows_of[s], "uint64_cas")}
            for s in series}
    for item in series:
        lines.append("| %s | %s |" % (item.label, " | ".join(
            "%.2f" % base[item][t]["ns_median"] if t in base[item] else "n/a"
            for t in threads)))
    for item in series:
        cells = []
        for t in threads:
            values = [base[s][t]["ns_median"] for s in series
                      if t in base[s]]
            if t in base[item] and values:
                cells.append("%.2f" % (base[item][t]["ns_median"]
                                       / (sum(values) / len(values))))
            else:
                cells.append("n/a")
        lines.append("| %s / mean | %s |" % (item.label, " | ".join(cells)))

    for operation in present_contended(series, rows_of):
        lines += ["", "## `%s()`, contended" % operation, "",
                  "Ratio to the baseline of the same block (median, with "
                  "the 25th and 75th percentiles of the repetitions; lower "
                  "is better).", "", head, rule]
        for item in series:
            by_threads = {r["threads"]: r
                          for r in contended(rows_of[item], operation)}
            lines.append("| %s | %s |" % (item.label, " | ".join(
                "%.1f (%.1f-%.1f)" % (by_threads[t]["ratio_median"],
                                      by_threads[t]["ratio_p25"],
                                      by_threads[t]["ratio_p75"])
                if t in by_threads else "n/a" for t in threads)))
        if operation == "compare_exchange_strong":
            lines += ["", "Share of successful compare-exchanges:", "",
                      head, rule]
            for item in series:
                by_threads = {r["threads"]: r
                              for r in contended(rows_of[item], operation)}
                lines.append("| %s | %s |" % (item.label, " | ".join(
                    "%.2f" % by_threads[t]["success_rate"]
                    if t in by_threads else "n/a" for t in threads)))

    has_cycles = any("cycles_median" in r for s in series
                     for r in rows_of[s])
    lines += ["", "## Uncontended", "",
              "One thread on a private object: median ns per operation, "
              "the 25th-75th percentile, and the ratio to the uncontended "
              "baseline of the same block."]
    if has_cycles:
        lines += ["The last two columns are the median cycles and "
                  "instructions per operation of the thread in user mode "
                  "(`perf_event_open`; 0 where the counter was not "
                  "available)."]
    lines += [""]
    if has_cycles:
        lines += ["| Operation | Series | Median, ns | p25-p75, ns "
                  "| x uint64 CAS | cycles | instructions |",
                  "| --- | --- | ---: | ---: | ---: | ---: | ---: |"]
    else:
        lines += ["| Operation | Series | Median, ns | p25-p75, ns "
                  "| x uint64 CAS |", "| --- | --- | ---: | ---: | ---: |"]
    for operation in ("uint64_cas",) + present(series, rows_of):
        for item in series:
            row = uncontended(rows_of[item], operation)
            if row is None:
                continue
            cells = ("| `%s` | %s | %.1f | %.1f-%.1f | %.2f |"
                     % (operation, item.label, row["ns_median"],
                        row["ns_p25"], row["ns_p75"],
                        row["ratio_median"]))
            if has_cycles:
                cells = cells[:-1] + " %.1f | %.1f |" % (
                    row["cycles_median"], row["instructions_median"])
            lines.append(cells)
    if any(r["operation"] == "one_writer_stores" for s in series
           for r in rows_of[s]):
        lines += rate_tables(series, rows_of, threads)
    lines.append("")
    return "\n".join(lines)


def rate_tables(series, rows_of, threads):
    """The read-heavy run as rates: loads per second of all the readers
    together, and stores per second of the writer (millions), from the median
    time per operation of each."""
    lines = ["", "## Read-heavy: one writer, the others load", "",
             "One thread stores alternating values in a loop, the other "
             "threads `load ()`. Millions of operations per second, from the "
             "median time per operation (all readers together for loads; "
             "the writer for stores). At 1 thread there is no contention: "
             "the thread alone loads (first table) or stores (second "
             "table). Higher is better.", ""]
    head = "| Series | " + " | ".join(str(t) for t in threads) + " |"
    rule = "| --- | " + " | ".join("---:" for _ in threads) + " |"
    for title, operation, readers in (
            ("Loads per second of the readers, millions", "load_one_writer",
             True),
            ("Stores per second of the writer, millions", "one_writer_stores",
             False)):
        lines += [title, "", head, rule]
        for item in series:
            by_threads = {r["threads"]: r
                          for r in contended(rows_of[item], operation)}
            cells = []
            for t in threads:
                if t not in by_threads or by_threads[t]["ns_median"] <= 0:
                    cells.append("n/a")
                    continue
                count = max(t - 1, 1) if readers else 1
                cells.append("%.2f" % (count * 1e3
                                       / by_threads[t]["ns_median"]))
            lines.append("| %s | %s |" % (item.label, " | ".join(cells)))
        lines.append("")
    return lines


def destroy_rows(rows, operation, thread_count):
    return {r["implementation"]: r for r in rows
            if r["mode"] == "destroy" and r["operation"] == operation
            and r["threads"] == thread_count}


def destroy_markdown(metas, series, rows_of):
    """The tables of the destruction-latency scenario."""
    rows = [r for s in series for r in rows_of[s]]
    counts = sorted({r["threads"] for r in rows if r["mode"] == "destroy"})
    lines = ["# Destruction latency", ""]
    for index, item in enumerate(metas):
        lines.append("- Run %d: %s, %s logical CPUs, governor `%s`; %s with "
                     "%s, %s build; %s repetitions of %s replaced objects "
                     "per series and thread count; measured %s."
                     % (index + 1, item.get("cpu", "?"),
                        item.get("logical_cpus", "?"),
                        item.get("governor", "?"), item.get("compiler", "?"),
                        item.get("library", "?"), item.get("build", "?"),
                        item.get("repetitions", "?"),
                        item.get("objects", "20000"), item.get("date", "?")))
    lines += ["", "A thread replaces the stored value with a new object "
              "that has a deleter, 20000 times in a row; the latency of an "
              "object is the time from the start of the store that replaced "
              "it to the run of its deleter (in nanoseconds). With more "
              "than one thread the others load in a loop, so the last "
              "owner can be a reader. The table gives, for each series, the "
              "median over the repetitions of: the median, the 99th "
              "percentile and the maximum of the latencies of the objects "
              "destroyed by the time the loop ended; the share of objects "
              "destroyed before the replacing store returned; the peak "
              "number of replaced objects not yet destroyed (after a "
              "store returned); and the share still pending when the loop "
              "ended (not in the percentiles).", ""]
    for count in counts:
        lines += ["## %d thread%s" % (count, "" if count == 1 else "s"), "",
                  "| Series | median, ns | p99, ns | max, ns "
                  "| destroyed in the call | peak backlog "
                  "| pending at the end |",
                  "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"]
        for item in series:
            by_operation = {op: destroy_rows(rows_of[item], op, count)
                            for op in DESTROY_OPERATIONS}
            median = by_operation["destroy_median"].get(item.key)
            if median is None:
                continue
            p99 = by_operation["destroy_p99"][item.key]
            high = by_operation["destroy_max"][item.key]
            backlog = by_operation["destroy_backlog"][item.key]
            lines.append("| %s | %.0f | %.0f | %.0f | %.1f %% | %.0f "
                         "| %.1f %% |"
                         % (item.label, median["ns_median"], p99["ns_median"],
                            high["ns_median"], 100 * median["success_rate"],
                            backlog["ns_median"],
                            100 * backlog["success_rate"]))
        lines.append("")
    return "\n".join(lines)


def destroy_charts(subtitle, series, rows_of, write):
    """One chart per statistic, a group per thread count."""
    rows = [r for s in series for r in rows_of[s]]
    counts = sorted({r["threads"] for r in rows if r["mode"] == "destroy"})
    for name, operation, label in (
            ("median", "destroy_median",
             "Median time from the replacing store to the destructor, ns"),
            ("p99", "destroy_p99",
             "99th percentile of the same time, ns"),
            ("backlog", "destroy_backlog",
             "Peak number of replaced objects not yet destroyed")):
        groups = []
        for count in counts:
            bars = []
            for item in series:
                row = destroy_rows(rows_of[item], operation, count).get(
                    item.key)
                if row is not None:
                    bars.append((item, row["ns_median"], row["ns_p25"],
                                 row["ns_p75"], "%.0f" % row["ns_median"]))
            groups.append(("%d thread%s" % (count, "" if count == 1 else "s"),
                           bars))
        write(name, bar_chart(
            label, [subtitle, "Median over the repetitions; whiskers: "
                              "25th-75th percentile"],
            label + " (lower is better)", groups))



def store_wait_rows(rows, operation, thread_count):
    return {r["implementation"]: r for r in rows
            if r["mode"] == "store_wait" and r["operation"] == operation
            and r["threads"] == thread_count}


def store_wait_series(series, rows_of):
    """The series that have store-wait rows, in the order of the file."""
    return [s for s in series
            if any(r["mode"] == "store_wait" for r in rows_of[s])]


def store_wait_counts(series, rows_of):
    return sorted({r["threads"] for s in series for r in rows_of[s]
                   if r["mode"] == "store_wait"})


def store_wait_cell(item, rows_of, operation, count, text_of):
    row = store_wait_rows(rows_of[item], operation, count).get(item.key)
    return text_of(row) if row is not None else "n/a"


def _switched(item, rows_of, count):
    """The bracket of the preemption table: slow stores after a switch."""
    row = store_wait_rows(rows_of[item], "store_wait_over_1ms_switched",
                          count).get(item.key)
    if row is None:
        return ""
    slow = store_wait_rows(rows_of[item], "store_wait_over_1ms",
                           count).get(item.key)
    return " (%d of %d)" % (row["successes_sum"],
                            slow["successes_sum"] if slow else 0)


def store_wait_markdown(metas, series, rows_of):
    """The tables of the store-wait scenario."""
    series = store_wait_series(series, rows_of)
    counts = store_wait_counts(series, rows_of)
    lines = ["# Time of a single store under a read load", ""]
    for index, item in enumerate(metas):
        lines.append("- Run %d: %s, %s logical CPUs, governor `%s`, kernel "
                     "`%s`; %s with %s, %s build; %s repetitions of %s "
                     "timed stores per series and thread count, %s s "
                     "pauses; clock `%s`; threads %s; measured %s in %s "
                     "minutes, load average %s at the start and %s at the "
                     "end."
                     % (index + 1, item.get("cpu", "?"),
                        item.get("logical_cpus", "?"),
                        item.get("governor", "?"), item.get("kernel", "?"),
                        item.get("compiler", "?"), item.get("library", "?"),
                        item.get("build", "?"), item.get("repetitions", "?"),
                        item.get("stores", "?"),
                        item.get("settle_seconds", "?"),
                        item.get("clock", "?"),
                        "pinned, one CPU each" if item.get("pinned") == "1"
                        else "not pinned", item.get("date", "?"),
                        item.get("minutes", "?"),
                        item.get("load_average_start", "?"),
                        item.get("load_average_end", "?")))
    lines += ["", "One thread (the writer) stores alternating values in a "
              "loop and times every `store ()` call on its own; the other "
              "threads (1 less than the thread count) `load ()` in a loop. "
              "Times are nanoseconds of one call, not rates. Every cell is "
              "the median over the repetitions of the statistic of one "
              "repetition, except the maximum, which also gives the "
              "longest store of all repetitions together, and the count of "
              "stores over 1 ms, which is summed over all repetitions "
              "(with its share of all stores timed). A store over 1 ms is "
              "far above any cost of the engine at these thread counts and "
              "is also what a preempted writer looks like: the count does "
              "not tell the two apart. Up to 20 threads are the 20 logical "
              "CPUs; 40 and 60 are oversubscribed.", ""]
    head = "| Series | " + " | ".join(str(t) for t in counts) + " |"
    rule = "| --- | " + " | ".join("---:" for _ in counts) + " |"
    for title, operation in (
            ("Median of the time of one store, ns", "store_wait_median"),
            ("90th percentile, ns", "store_wait_p90"),
            ("99th percentile, ns", "store_wait_p99"),
            ("99.9th percentile, ns", "store_wait_p999"),
            ("99.99th percentile, ns", "store_wait_p9999")):
        lines += ["## " + title, "", head, rule]
        for item in series:
            lines.append("| %s | %s |" % (item.label, " | ".join(
                store_wait_cell(item, rows_of, operation, t,
                                lambda r: "%.0f" % r["ns_median"])
                for t in counts)))
        lines.append("")
    lines += ["## The longest store, ns", "",
              "Median over the repetitions of the longest store of a "
              "repetition, then the longest store of all repetitions "
              "together in brackets.", "", head, rule]
    for item in series:
        lines.append("| %s | %s |" % (item.label, " | ".join(
            store_wait_cell(item, rows_of, "store_wait_max", t,
                            lambda r: "%.0f (%.0f)" % (r["ns_median"],
                                                       r["ns_max"]))
            for t in counts)))
    lines += ["", "## Stores over 1 ms", "",
              "Stores that took more than a millisecond, summed over all "
              "the repetitions, with the share of all stores timed.", "",
              head, rule]
    for item in series:
        lines.append("| %s | %s |" % (item.label, " | ".join(
            store_wait_cell(
                item, rows_of, "store_wait_over_1ms", t,
                lambda r: "%d (%.4f %%)" % (
                    r["successes_sum"],
                    100.0 * r["successes_sum"] / max(r["operations_sum"], 1)))
            for t in counts)))
    lines += ["", "## Preemption of the writer", "",
              "Involuntary context switches of the writer thread during "
              "its timed stores (the scheduler took it off its CPU), "
              "summed over all the repetitions, and in brackets how many "
              "of the stores over 1 ms were followed by an advance of that "
              "counter since the previous reading (the counter is read "
              "only after a store over 1 ms, so a switch that took less "
              "than 1 ms can be credited to the next slow store: the "
              "bracket is an upper bound of the slow stores that a "
              "preemption explains). Where it is 0 on "
              "a platform, the system does not report it.", "", head, rule]
    for item in series:
        lines.append("| %s | %s |" % (item.label, " | ".join(
            store_wait_cell(
                item, rows_of, "store_wait_switches", t,
                lambda r: "%d" % r["successes_sum"])
            + _switched(item, rows_of, t)
            for t in counts)))
    lines += ["", "## Cost of the timer, ns", "",
              "Median of an empty pair of `steady_clock::now ()` reads on "
              "the writer thread, with the readers running, right before "
              "its stores; the median over the repetitions. It is part of "
              "every time above.", "", head, rule]
    for item in series:
        lines.append("| %s | %s |" % (item.label, " | ".join(
            store_wait_cell(item, rows_of, "store_wait_timer", t,
                            lambda r: "%.0f" % r["ns_median"])
            for t in counts)))
    lines.append("")
    return "\n".join(lines)


def store_wait_charts(subtitle, series, rows_of, write):
    """One chart per statistic, the thread count along the axis."""
    series = store_wait_series(series, rows_of)
    counts = store_wait_counts(series, rows_of)
    for name, operation, title, overall in (
            ("median", "store_wait_median",
             "Median time of one store() under a read load, ns", False),
            ("p99", "store_wait_p99",
             "99th percentile of the time of one store(), ns", False),
            ("p9999", "store_wait_p9999",
             "99.99th percentile of the time of one store(), ns", False),
            ("max", "store_wait_max",
             "Longest store() of all repetitions, ns", True)):
        lines = []
        for item in series:
            points = []
            for count in counts:
                row = store_wait_rows(rows_of[item], operation, count).get(
                    item.key)
                if row is None:
                    continue
                if overall:
                    points.append((count, row["ns_max"], row["ns_max"],
                                   row["ns_max"]))
                else:
                    points.append((count, row["ns_median"], row["ns_p25"],
                                   row["ns_p75"]))
            lines.append((item, points))
        note = ("Longest single store of all repetitions together "
                "(the starvation evidence; a store preempted by the "
                "scheduler counts too)" if overall else
                "Median over the repetitions; whiskers: 25th-75th "
                "percentile")
        write(name, line_chart(
            title, [subtitle, note + "; logarithmic axis"],
            "nanoseconds per store (lower is better)", lines, log=True))


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    args = argv[1:]
    prefix = ""
    if args and args[0] == "--prefix":
        if len(args) < 3:
            print(__doc__)
            return 2
        prefix = args[1]
        args = args[2:]
    paths = args
    metas, series, rows_of = load_all(paths)
    stem = os.path.splitext(paths[0])[0]
    image_stem = os.path.join(os.path.dirname(stem),
                              prefix + os.path.basename(stem))
    subtitle = describe(metas[0]) if len(metas) == 1 else (
        "%d result files; ratios to each process's own baseline"
        % len(metas))
    written = []

    def write(name, content):
        path = "%s_%s.svg" % (image_stem, name)
        with open(path, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
        written.append(path)

    if metas[0].get("scenario") == "destroy":
        destroy_charts(subtitle, series, rows_of, write)
        with open(stem + ".md", "w", encoding="utf-8", newline="\n") as out:
            out.write(destroy_markdown(metas, series, rows_of))
        written.append(stem + ".md")
        print("written " + ", ".join(written))
        return 0

    if metas[0].get("scenario") == "store_wait":
        store_wait_charts(subtitle, series, rows_of, write)
        with open(stem + ".md", "w", encoding="utf-8", newline="\n") as out:
            out.write(store_wait_markdown(metas, series, rows_of))
        written.append(stem + ".md")
        print("written " + ", ".join(written))
        return 0

    write("baseline", line_chart(
        "Baseline: std::atomic<uint64_t>::compare_exchange_strong, contended",
        [subtitle, "Timed right before each series' block: the same code "
                   "every time, so the lines should overlap; whiskers: "
                   "25th-75th percentile"],
        "ns per operation and thread",
        [(s, [(r["threads"], r["ns_median"], r["ns_p25"], r["ns_p75"])
              for r in contended(rows_of[s], "uint64_cas")])
         for s in series]))
    for operation in present_contended(series, rows_of):
        write(operation, line_chart(
            "%s() under contention, relative to the baseline" % operation,
            [subtitle, "Each point: time per operation / uint64 CAS time of "
                       "the same block; whiskers: 25th-75th percentile"],
            "time / uint64 CAS time (lower is better)",
            [(s, [(r["threads"], r["ratio_median"], r["ratio_p25"],
                   r["ratio_p75"])
                  for r in contended(rows_of[s], operation)])
             for s in series],
            reference=1.0, reference_label="uint64 CAS = 1"))
    groups = []
    for operation in present(series, rows_of):
        bars = []
        for item in series:
            row = uncontended(rows_of[item], operation)
            if row is not None:
                bars.append((item, row["ns_median"], row["ns_p25"],
                             row["ns_p75"], "%.1f ns (%.1fx CAS)"
                             % (row["ns_median"], row["ratio_median"])))
        groups.append((operation + "()", bars))
    write("uncontended", bar_chart(
        "Uncontended: one thread, private object",
        [subtitle, "Median ns per operation, ratio to the uncontended "
                   "uint64 CAS in brackets; whiskers: 25th-75th percentile"],
        "nanoseconds per operation (lower is better)", groups))
    text_of_md = markdown(metas, series, rows_of)
    compile_path = os.path.join(os.path.dirname(os.path.abspath(paths[0])),
                                "compile_time.csv")
    if os.path.exists(compile_path):
        compile_comments, compile_rows = read_compile(compile_path)
        path = os.path.join(os.path.dirname(stem),
                            prefix + "atomic_compile_time.svg")
        with open(path, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(compile_chart(compile_rows, compile_comments))
        written.append(path)
        text_of_md = "\n".join([text_of_md.rstrip("\n")] + compile_markdown(
            compile_rows, compile_comments))
    with open(stem + ".md", "w", encoding="utf-8", newline="\n") as stream:
        stream.write(text_of_md)
    written.append(stem + ".md")
    print("written " + ", ".join(written))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
