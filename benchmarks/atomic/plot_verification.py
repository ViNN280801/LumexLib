#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Turns the verification data of the split-count engine into SVG charts.

Standard library only (no matplotlib). The style, the palette, the font and the
helper functions are those of plot_results.py, which this script imports from
its own directory. The output is deterministic: the same CSV files give the
same bytes.

Usage:
    python plot_verification.py [results/verification]

Reads, in the given directory (default: results/verification next to this
script):
    mutants.csv           the mutations of every layer and the layers that
                          caught them
    relacy_models.csv     the Relacy models: threads, search, bound, number of
                          executions, seconds
    sanitizer_matrix.csv  the sanitizer matrix: every row, run and result

Writes, in the same directory (the names start with `verification_` because an
image name must be unique in the documentation):
    verification_mutants.svg           a heatmap: mutation by layer
    verification_relacy_executions.svg executions of every Relacy model
    verification_sanitizer_matrix.svg  a grid: matrix row by run
"""

import csv
import html
import math
import os
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from plot_results import CHAR_WIDTH, nice_ticks, svg_open, text  # noqa: E402

# The states of a cell of the mutation heatmap: (letter, color, meaning).
STATES = [
    ("caught", "C", "#009E73", "caught"),
    ("soak", "S", "#56B4E9", "caught only at the soak bound"),
    ("aggregate", "A", "#8FD3B8",
     "caught, known from an aggregate statement (no per-mutant record)"),
    ("partial", "P", "#E69F00", "caught in one counter flavour only"),
    ("missed", "X", "#D55E00", "not caught (survivor or equivalent mutant)"),
]
STATE_OF = {name: (letter, color, meaning)
            for name, letter, color, meaning in STATES}
EMPTY_COLOR = "#edf2f7"
LAYERS = [("v1", "V1 simulator"), ("v2", "V2 ABA"), ("v3", "V3 family"),
          ("v4", "V4 Relacy"), ("other", "unit suites")]
GROUPS = [("prereg", "pre-registered"), ("V2", "V2 campaign"),
          ("V3", "V3 review"), ("V4", "V4 own")]
SEARCH_COLORS = {"full": "#009E73", "bound": "#0072B2", "random": "#E69F00",
                 "control": "#CC79A7"}
SEARCH_LABELS = {"full": "full search (every schedule)",
                 "bound": "preemption bound search",
                 "random": "random schedules",
                 "control": "control: the weak outcome is found"}


def read_csv(path):
    with open(path, newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream))


def fit(label, columns):
    """Cuts `label` to about `columns` average glyph widths."""
    if len(label) <= columns:
        return label
    return label[:columns - 3].rstrip() + "..."


def finish(out, width, height):
    out[1] = out[1].replace('0 0 %d 0"' % width, '0 0 %d %d"'
                            % (width, height)).replace(
        'height="0"', 'height="%d"' % height)
    out.append("</svg>")
    return "\n".join(out) + "\n"


def heading(out, title, subtitle):
    out.append(text(20, 30, title, size=18, weight="600"))
    for index, line in enumerate(subtitle):
        out.append(text(20, 52 + 17 * index, line, size=12, color="#4a5568"))
    return 52 + 17 * len(subtitle)


def swatch_legend(out, items, x0, y0, max_width):
    """Writes (color, letter, label) items as a wrapping legend; returns y."""
    x = float(x0)
    y = float(y0)
    for color, letter, label in items:
        width = 30 + CHAR_WIDTH * len(label) + 22
        if x + width > x0 + max_width and x > x0:
            x = float(x0)
            y += 22
        out.append('<rect x="%.1f" y="%.1f" width="16" height="14" rx="2" '
                   'fill="%s"/>' % (x, y - 12, color))
        if letter:
            out.append(text(x + 8, y - 1, letter, size=10, anchor="middle",
                            weight="600", color="#ffffff"))
        out.append(text(x + 22, y, label, size=12))
        x += width
    return y + 22


# -- The heatmap of the mutations --


def mutant_rows(rows):
    """The rows that the heatmap draws, with their group.

    The two halves of M33 (rows of the Relacy layer) follow M33 in the
    pre-registered group; the Relacy rows that carry a number of the
    pre-registered list are drawn there, not twice."""
    prereg_ids = {r["id"] for r in rows if r["layer"] == "prereg"}
    halves = []
    for row in rows:
        if (row["layer"] == "V4" and row["id"].startswith("M33")
                and row["id"] not in prereg_ids
                and row["id"] not in [h["id"] for h in halves]):
            halves.append(row)
    drawn = []
    seen = set()
    for group, _ in GROUPS:
        for row in rows:
            if row["layer"] != group:
                continue
            if group == "V4" and (row["id"] in prereg_ids
                                  or row["id"] in seen
                                  or row["id"].startswith("M33")):
                continue
            seen.add(row["id"])
            drawn.append((group, row))
            if group == "prereg" and row["id"] == "M33":
                for half in halves:
                    drawn.append((group, half))
    return drawn


def mutants_chart(rows):
    drawn = mutant_rows(rows)
    cell_w = 74
    cell_h = 15
    label_w = 400
    group_w = 20
    margin = 20
    width = margin + group_w + label_w + cell_w * len(LAYERS) + margin
    out = svg_open(width, 0, "Mutations and the layers that caught them")
    y = heading(out, "Mutations and the layers that caught them", [
        "One row per mutation. A cell is the verdict of one layer; a blank cell means that layer was",
        "not run on it, or has no record. Rows: the pre-registered M1-M38 (with the two halves of M33),",
        "the 24 mutants of the ABA campaign, the 5 of the family-suite review, and the 5 own mutants",
        "V4/101 to V4/105 of the Relacy models."])
    items = [(color, letter, meaning)
             for _, letter, color, meaning in STATES]
    items.append((EMPTY_COLOR, "", "not run, or no record"))
    y = swatch_legend(out, items, margin, y + 22, width - 2 * margin)
    top = y + 26
    # Column headers.
    for index, (_, label) in enumerate(LAYERS):
        x = margin + group_w + label_w + cell_w * index + cell_w / 2.0
        out.append(text(x, top - 8, label, size=11, anchor="middle",
                        weight="600", color="#2d3748"))
    y = float(top)
    last_group = None
    group_tops = {}
    for group, row in drawn:
        if group != last_group:
            if last_group is not None:
                y += 8
            group_tops[group] = y
            last_group = group
        label = "%s  %s" % (row["id"], row["description"])
        out.append(text(margin + group_w, y + cell_h - 4,
                        fit(label, int(label_w / CHAR_WIDTH * 1.12)),
                        size=11))
        for index, (key, layer_label) in enumerate(LAYERS):
            x = margin + group_w + label_w + cell_w * index
            state = row[key]
            if state in STATE_OF:
                letter, color, meaning = STATE_OF[state]
                tip = "%s, %s: %s" % (row["id"], layer_label, meaning)
            else:
                letter, color = "", EMPTY_COLOR
                tip = "%s, %s: not run, or no record" % (row["id"],
                                                         layer_label)
            out.append('<rect x="%d" y="%.1f" width="%d" height="%d" rx="2" '
                       'fill="%s"><title>%s</title></rect>'
                       % (x + 2, y + 1, cell_w - 4, cell_h - 2, color,
                          html.escape(tip)))
            if letter:
                out.append(text(x + cell_w / 2.0, y + cell_h - 4, letter,
                                size=10, anchor="middle", weight="600",
                                color="#ffffff"))
        y += cell_h
    # Group labels (rotated) and separators.
    ordered = [g for g, _ in GROUPS if g in group_tops]
    names = dict(GROUPS)
    for position, group in enumerate(ordered):
        start = group_tops[group]
        stop = group_tops[ordered[position + 1]] - 8 \
            if position + 1 < len(ordered) else y
        middle = (start + stop) / 2.0
        out.append('<text x="%d" y="%.1f" font-size="11" '
                   'text-anchor="middle" font-weight="600" fill="#2d3748" '
                   'transform="rotate(-90 %d %.1f)">%s</text>'
                   % (margin + 8, middle, margin + 8, middle,
                      html.escape(names[group])))
        out.append('<line x1="%d" y1="%.1f" x2="%d" y2="%.1f" '
                   'stroke="#a0aec0" stroke-width="1"/>'
                   % (margin + group_w - 4, start,
                      margin + group_w - 4, stop))
    return finish(out, width, int(y + 24))


# -- The bar chart of the Relacy executions --


def relacy_chart(rows):
    rows = [r for r in rows if r["counter"] == "library"]
    bar = 12
    gap = 3
    label_w = 330
    plot_w = 430
    value_w = 90
    margin = 20
    width = margin + label_w + plot_w + value_w + margin
    out = svg_open(width, 0, "Executions of the Relacy models")
    y = heading(out, "Executions of the Relacy models", [
        "One bar per model and search (library counter; the ThreadSanitizer "
        "counter gives the same numbers). Logarithmic axis.",
        "A full search ends when every schedule of the restricted reads-from "
        "relation was run; the others stop at their bound or count."])
    items = [(SEARCH_COLORS[key], "", SEARCH_LABELS[key])
             for key in ("full", "bound", "random", "control")]
    y = swatch_legend(out, items, margin, y + 22, width - 2 * margin)
    top = y + 6
    entries = []
    for row in rows:
        name = row["model"]
        if row["reserve"]:
            name += "/k" + row["reserve"]
        kind = row["search"]
        if row["outcome"].startswith("violation"):
            kind = "control"
        search = "bound %s" % row["bound"] if kind == "bound" else kind
        entries.append((name, search, kind, int(row["executions"])))
    top_value = max(e[3] for e in entries)
    decades = int(math.ceil(math.log10(top_value)))
    plot_x = margin + label_w

    def x_of(value):
        return plot_x + plot_w * math.log10(max(value, 1)) / decades

    y = float(top)
    for index, (name, search, kind, value) in enumerate(entries):
        if index % 2 == 0:
            out.append('<rect x="%d" y="%.1f" width="%d" height="%d" '
                       'fill="#f7fafc"/>'
                       % (margin, y - 1, label_w + plot_w + value_w,
                          bar + gap))
        out.append(text(margin + 4, y + bar - 2, "%s  %s" % (name, search),
                        size=11))
        out.append('<rect x="%d" y="%.1f" width="%.1f" height="%d" rx="2" '
                   'fill="%s"><title>%s</title></rect>'
                   % (plot_x, y, max(1.0, x_of(value) - plot_x), bar,
                      SEARCH_COLORS[kind],
                      html.escape("%s, %s: %d executions" % (name, search,
                                                              value))))
        out.append(text(x_of(value) + 6, y + bar - 2, "{:,}".format(value),
                        size=10, color="#2d3748"))
        y += bar + gap
    plot_bottom = y
    for power in range(decades + 1):
        x = x_of(10 ** power)
        out.append('<line x1="%.1f" y1="%d" x2="%.1f" y2="%.1f" '
                   'stroke="#cbd5e0" stroke-width="1"/>'
                   % (x, top - 2, x, plot_bottom))
        out.append(text(x, plot_bottom + 16, "10^%d" % power, size=11,
                        anchor="middle", color="#4a5568"))
    out.append(text(plot_x + plot_w / 2.0, plot_bottom + 36,
                    "executions (log scale)", anchor="middle",
                    color="#2d3748"))
    return finish(out, width, int(plot_bottom + 52))


# -- The grid of the sanitizer matrix --

GRID_COLUMNS = [("first run", "default", "first run, default"),
                ("first run", "soak", "first run, soak"),
                ("after the fixes", "default", "after the fixes, default"),
                ("after the fixes", "soak", "after the fixes, soak")]
PASS_COLOR = "#009E73"
FAIL_COLOR = "#D55E00"
PARTIAL_COLOR = "#E69F00"
NONE_COLOR = "#edf2f7"


def number(value):
    """An int of a CSV field, or None for 'not recorded'."""
    try:
        return int(value)
    except ValueError:
        return None


def counts(row):
    """Text of one run: passed, failed and skipped of its tests."""
    tests = int(row["tests"])
    failed = int(row["failed"])
    skipped = number(row["skipped"])
    passed = number(row["passed"])
    if skipped is None:
        return "%d failed of %d (skips not recorded)" % (failed, tests) \
            if failed else "%d tests, none failed (skips not recorded)" \
            % tests
    if failed:
        return "%d failed, %d passed, %d skipped (of %d)" % (
            failed, passed, skipped, tests)
    return "%d passed, %d skipped (of %d)" % (passed, skipped, tests)


def cell_of(rows, phase, kind):
    """(color, text, tip) of the runs of one phase and kind of one row."""
    chosen = [r for r in rows if r["phase"] == phase
              and r["run"].split(" ")[0] == kind]
    if not chosen:
        return NONE_COLOR, "not run", "not run in this phase"
    if len(chosen) > 1:
        parts = "; ".join("part %d: %s" % (index + 1, counts(r))
                          for index, r in enumerate(chosen))
        return PARTIAL_COLOR, "in %d parts" % len(chosen), \
            "run in %d parts, the second re-runs tests of the first (%s)" \
            % (len(chosen), parts)
    row = chosen[0]
    failed = int(row["failed"])
    reps = int(row["repetitions"])
    suffix = " x%d" % reps if reps > 1 else ""
    skipped = number(row["skipped"])
    if failed:
        text = "%d failed of %s" % (failed, row["tests"])
        color = FAIL_COLOR
    elif skipped is None:
        text = "%s tests, none failed" % row["tests"]
        color = PASS_COLOR
    else:
        text = "%s passed, %d skipped" % (row["passed"], skipped)
        color = PASS_COLOR
    return color, text + suffix, "%s; %d run(s); %s" % (
        counts(row), reps, row["note"] or "no note")


def sanitizer_chart(rows):
    order = []
    for row in rows:
        if row["row"] not in order:
            order.append(row["row"])
    cell_w = 190
    cell_h = 22
    label_w = 330
    margin = 20
    width = margin + label_w + cell_w * len(GRID_COLUMNS) + margin
    out = svg_open(width, 0, "The sanitizer matrix")
    y = heading(out, "The sanitizer matrix", [
        "One row per compiler, library and sanitizer; the columns are those of the table in the README.",
        "A cell is the passed, failed and skipped tests of the run; xN = N seeds. Skipped tests are not passed tests.",
        "The first run found two defects of the tests (UBSan, TSan); the 'after the fixes' columns are the re-runs."])
    items = [(PASS_COLOR, "", "all tests passed or were skipped by design"),
             (FAIL_COLOR, "", "tests failed"),
             (PARTIAL_COLOR, "", "completed in two parts (row 1, soak: part 2 re-runs 256 tests of part 1)"),
             (NONE_COLOR, "", "not run")]
    y = swatch_legend(out, items, margin, y + 22, width - 2 * margin)
    top = y + 34
    for index, (_, _, label) in enumerate(GRID_COLUMNS):
        x = margin + label_w + cell_w * index + cell_w / 2.0
        out.append(text(x, top - 8, label, size=11, anchor="middle",
                        weight="600", color="#2d3748"))
    y = float(top)
    for key in order:
        group = [r for r in rows if r["row"] == key]
        head = group[0]
        label = "%s  %s, %s, %s, %s" % (key, head["compiler"], head["stdlib"],
                                        head["sanitizer"], head["build_type"])
        out.append(text(margin, y + cell_h - 7,
                        fit(label, int(label_w / CHAR_WIDTH * 1.12)),
                        size=11))
        for index, (phase, kind, column) in enumerate(GRID_COLUMNS):
            x = margin + label_w + cell_w * index
            color, content, tip = cell_of(group, phase, kind)
            out.append('<rect x="%d" y="%.1f" width="%d" height="%d" rx="3" '
                       'fill="%s"><title>%s</title></rect>'
                       % (x + 2, y + 1, cell_w - 4, cell_h - 2, color,
                          html.escape("%s, %s: %s" % (key, column, tip))))
            ink = "#2d3748" if color in (NONE_COLOR, PARTIAL_COLOR) \
                else "#ffffff"
            out.append(text(x + cell_w / 2.0, y + cell_h - 7, content,
                            size=11, anchor="middle", weight="600",
                            color=ink))
        y += cell_h
    return finish(out, width, int(y + 24))


def main(argv):
    here = os.path.dirname(os.path.abspath(__file__))
    directory = argv[1] if len(argv) > 1 else os.path.join(
        here, "results", "verification")
    jobs = [("mutants.csv", mutants_chart, "verification_mutants.svg"),
            ("relacy_models.csv", relacy_chart,
             "verification_relacy_executions.svg"),
            ("sanitizer_matrix.csv", sanitizer_chart,
             "verification_sanitizer_matrix.svg")]
    written = []
    for source, chart, name in jobs:
        rows = read_csv(os.path.join(directory, source))
        path = os.path.join(directory, name)
        with open(path, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(chart(rows))
        written.append(path)
    print("written " + ", ".join(written))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
