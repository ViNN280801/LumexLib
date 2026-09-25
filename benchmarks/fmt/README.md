# LumexFormat benchmarks

Throughput of `lumex::core::fmt` (LumexFormat) against `std::format`,
`std::ostringstream`, `std::snprintf` and `std::to_string` on the same
scenarios. Every method of a scenario produces byte-identical text; the
benchmark checks that before timing, so the numbers compare equal work.

![Time per call](results/format_benchmark.svg)

![Time relative to LumexFormat](results/format_benchmark_relative.svg)

The numbers behind the charts: [results/format_benchmark.md](results/format_benchmark.md)
(raw data: [results/format_benchmark.csv](results/format_benchmark.csv)).

## Method

- Scenarios: an `int` with `{}`, an `int` with `{:#010x}`, a `double` with
  `{:.3f}` and with `{}` (shortest round trip), a padded `std::string`, a
  typical log line with three fields, ten fields, and `format_to` into a
  reused `std::string`.
- Each (scenario, method) runs a warm-up of 10 % of the iterations, then
  15 repetitions of 200 000 calls; the CSV keeps the median, minimum and
  maximum time per call. `--quick` runs 3 x 20 000 calls for a smoke check.
- The results are only meaningful for a Release build without sanitizers,
  on an otherwise idle machine.
- `to_string (6 digits)` in `double_shortest` is shown for scale only: it
  prints 6 fixed digits, not the shortest text.

## Running

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DLUMEX_BUILD_BENCHMARKS=ON
cmake --build build-bench --target LumexFmtBenchmarkRun
```

`LumexFmtBenchmarkRun` builds `LumexFmtBenchmark`, writes
`results/format_benchmark.csv` and runs `plot_results.py`, which needs only
the Python standard library and writes the two SVG charts and the Markdown
table next to the CSV. Separately:

```sh
build-bench/bin/LumexFmtBenchmark benchmarks/fmt/results/format_benchmark.csv
python benchmarks/fmt/plot_results.py benchmarks/fmt/results/format_benchmark.csv
```

## Reading the results (MSVC 19.51, x64, Release)

- LumexFormat is 2 to 6 times faster than `std::ostringstream` in every
  scenario.
- Against MSVC's `std::format` it is faster for hexadecimal and padded
  strings and 1.2 to 1.5 times slower for decimal integers, floating point
  and many-field lines (per-field parsing and argument dispatch are not
  tuned the way the standard library is).
- `snprintf` stays the fastest for plain numbers, without type safety or
  compile-time checks.
