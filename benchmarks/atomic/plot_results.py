#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the results of run_benchmark.py into SVG charts and a table.

Standard library only (no matplotlib). The charts are vector SVG with a
viewBox, so they stay sharp at any zoom, in a browser, an IDE, the
repository web view and the Doxygen documentation.

Usage:
    python plot_results.py results/atomic_benchmark.csv [more.csv ...]

More files (for example a run on another machine or compiler) add their
series to the same charts; a ratio to the baseline of its own process is
what makes series from different processes and machines comparable.

Writes next to the first CSV:
    atomic_benchmark_baseline.svg      the uint64 CAS baseline before each
                                       implementation (should overlap)
    atomic_benchmark_<operation>.svg   load, store, exchange, load_one_writer and
                                       compare_exchange_strong under
                                       contention, as ratios to the baseline
    atomic_benchmark_uncontended.svg   one thread, nanoseconds
    atomic_benchmark.md                the numbers as Markdown tables
"""

import csv
import html
import math
import os
import sys

OPERATIONS = ("load", "store", "exchange", "compare_exchange_strong",
              "load_one_writer")
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
}
FALLBACK_COLORS = ["#8c564b", "#7f7f7f", "#17becf", "#bcbd22"]
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
                    "ns_p25", "ns_p75", "ns_min", "ns_max", "success_rate"):
            row[key] = float(row[key])
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
               reference_label=""):
    """`lines`: [(Series, [(threads, value, low, high)])] -> SVG text.

    `subtitle` is a list of lines.
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
    ticks = nice_ticks(max(highs) if highs else 1.0)
    x_min, x_max = (xs[0], xs[-1]) if xs else (0, 1)
    span = max(x_max - x_min, 1)

    def x_of(value):
        return plot_left + 20 + (plot_width - 40) * (value - x_min) / span

    def y_of(value):
        return bottom - plot_height * value / ticks[-1]

    for tick in ticks:
        y = y_of(tick)
        out.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" '
                   'stroke="#e2e8f0" stroke-width="1"/>'
                   % (plot_left, y, plot_left + plot_width, y))
        out.append(text(plot_left - 8, y + 4, "%g" % tick, size=11,
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


def present(series, rows_of):
    """The operations some series has rows for (older files lack newer ones)."""
    return tuple(op for op in OPERATIONS
                 if any(r["operation"] == op for s in series
                        for r in rows_of[s]))


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

    for operation in present(series, rows_of):
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

    lines += ["", "## Uncontended", "",
              "One thread on a private object: median ns per operation, "
              "the 25th-75th percentile, and the ratio to the uncontended "
              "baseline of the same block.", "",
              "| Operation | Series | Median, ns | p25-p75, ns "
              "| x uint64 CAS |", "| --- | --- | ---: | ---: | ---: |"]
    for operation in ("uint64_cas",) + present(series, rows_of):
        for item in series:
            row = uncontended(rows_of[item], operation)
            if row is None:
                continue
            lines.append("| `%s` | %s | %.1f | %.1f-%.1f | %.2f |"
                         % (operation, item.label, row["ns_median"],
                            row["ns_p25"], row["ns_p75"],
                            row["ratio_median"]))
    lines.append("")
    return "\n".join(lines)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    paths = argv[1:]
    metas, series, rows_of = load_all(paths)
    stem = os.path.splitext(paths[0])[0]
    subtitle = describe(metas[0]) if len(metas) == 1 else (
        "%d result files; ratios to each process's own baseline"
        % len(metas))
    written = []

    def write(name, content):
        path = "%s_%s.svg" % (stem, name)
        with open(path, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
        written.append(path)

    write("baseline", line_chart(
        "Baseline: std::atomic<uint64_t>::compare_exchange_strong, contended",
        [subtitle, "Timed right before each series' block: the same code "
                   "every time, so the lines should overlap; whiskers: "
                   "25th-75th percentile"],
        "ns per operation and thread",
        [(s, [(r["threads"], r["ns_median"], r["ns_p25"], r["ns_p75"])
              for r in contended(rows_of[s], "uint64_cas")])
         for s in series]))
    for operation in present(series, rows_of):
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
    with open(stem + ".md", "w", encoding="utf-8", newline="\n") as stream:
        stream.write(markdown(metas, series, rows_of))
    written.append(stem + ".md")
    print("written " + ", ".join(written))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
