# span benchmarks

The span of this library (`lumex::core::span::view::span`) against `std::span`
of the standard library in use and `boost::span` of Boost.Core, on the same
scenarios and the same source. The span of this library is measured at C++11
and at C++20; `std::span` at C++20; `boost::span` at C++11 and at C++20. A
standard library needs its own compiler, so there are two toolchains:

- **GCC** with libstdc++ (`std::span` of libstdc++),
- **Clang** with libc++ (`std::span` of libc++).

![Time relative to lumex C++11, GCC](results/span_benchmark_gcc.svg)

![Time relative to lumex C++11, Clang](results/span_benchmark_clang.svg)

![Compile time](results/compile_time.svg)

The numbers behind the charts: [results/span_benchmark.md](results/span_benchmark.md)
(raw data: [results/span_benchmark.csv](results/span_benchmark.csv), compile
times: [results/compile_time.csv](results/compile_time.csv)).

## Method

- One source, `bench_span.cpp`, compiled once per implementation and standard
  with `-DBENCH_SPAN_IMPL=1|2|3` (`bench_span_impl.hpp` maps it to the span
  under test). Every variant of a toolchain is built with the same flags of the
  Release profile of the library; there are no sanitizers.
- Scenarios (the unit of an operation is in the description column of the
  results): construction from a pointer and a count, a built-in array, a
  `std::array`, a `std::vector` and a static-extent array; copy; sums by
  range-for, by `operator[]` and by iterators over 8, 1 024 and 1 000 000
  ints and over a static extent; `first`, `last`, `subspan` with run-time and
  `first<N>`, `subspan<O, N>` with compile-time counts; static to dynamic,
  dynamic to static and mutable to const conversions; passing a span by value
  to a `noinline` and to an inlined function; a recursive sum with `first` /
  `subspan` halves down to one element (many tiny spans); `as_bytes` sums;
  `size_bytes`; `front` and `back`.
- Equal work: before timing, every scenario is run for a few iterations and its
  checksum is compared with a reference that uses raw pointers only. The
  checksums are written to the CSV and `run_benchmark.py` compares them across
  all executables of all toolchains; different checksums stop the run.
- The optimizer is kept from deleting or hoisting the work with empty `asm`
  statements that read the spans (`"m"`) and clobber memory, and with values
  passed through `asm` so that sizes and pointers are not known at compile time
  (`_ReadWriteBarrier` and a volatile on MSVC).
- A scenario is calibrated until one run lasts at least 20 ms, then runs once
  as warm-up and 15 times for the numbers. The CSV keeps the median, the
  minimum and the maximum time per operation of the 15 repetitions;
  `run_benchmark.py` runs every executable `--passes` times (default 5), one
  pass over all executables after the other, and keeps the median of the
  medians, the smallest minimum and the largest maximum. `--quick` runs one
  pass with 3 short repetitions as a smoke check.
- The process is pinned to one CPU with `taskset` (`--cpu`) when available.
- Compile time: `compile_time.cpp` includes the span header and uses the whole
  API; `run_benchmark.py --compile-time` measures the wall time of
  `-fsyntax-only -O0` of that unit per compiler and variant (median of the
  repeats), next to the same unit with a stub instead of a span.
- The results are only meaningful for a Release build without sanitizers, on
  an otherwise idle machine.

## Boost

Boost is optional and header-only: the boost variants need `boost/core/span.hpp`
(Boost 1.78 or later) and are skipped, with a message, when CMake finds none.
Pass the include directory with `-DLUMEX_BENCH_BOOST_INCLUDE_DIR=<dir>` or point
`find_package(Boost 1.78 CONFIG)` at an installation (`CMAKE_PREFIX_PATH`,
`Boost_DIR`). Nothing of Boost is linked or vendored. On the workstation of the
committed results Boost 1.92.0 is installed in `/opt/boost-1.92.0` (built with
the system GCC 8; only its headers are used).

## Running

```sh
cmake -S . -B build-bench-gcc -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_CXX_COMPILER=g++ -DLUMEX_BUILD_BENCHMARKS=ON \
      -DLUMEX_BENCH_BOOST_INCLUDE_DIR=/opt/boost-1.92.0/include
cmake --build build-bench-gcc
# the same with clang++ (libc++) in build-bench-clang
python benchmarks/span/run_benchmark.py \
       --build-dir gcc=build-bench-gcc --build-dir clang=build-bench-clang \
       --compile-time --compiler "gcc=g++" --compiler "clang=clang++ -stdlib=libc++" \
       --boost-include /opt/boost-1.92.0/include --cpu 3
```

`run_benchmark.py` writes `results/span_benchmark.csv`, `results/compile_time.csv`
and, through `plot_results.py` (Python standard library only), the Markdown
table and the SVG charts. The target `LumexSpanBenchmarkRun` runs the
executables of one build tree and writes into that build tree, so it never
overwrites the committed results; the committed results need both toolchains
as above.

## Reading the results

Measured on an Intel Core i7-12700K (Linux 6.1, pinned to one performance
core), GCC 13.2.0 with libstdc++ and Clang 23.1.0 with libc++, Boost 1.92.0
(headers only), 5 passes, in October 2026. The full tables are in
[results/span_benchmark.md](results/span_benchmark.md); the main findings:

- **Same standard, same toolchain: on par.** Comparing the columns of one C++
  standard, the span of this library is within 13 % of `std::span` and of
  `boost::span` in every scenario (36; 34 against `boost::span` at C++11,
  which has no `as_bytes`), and within 10 % in all of them but two entries of
  one pair: lumex C++11 against `boost::span` C++11 with Clang, `ctor_vector`
  1.13x and `sum_iterators (1,024)` 0.82x. The geometric mean of the
  time ratio, lumex C++20 against `std::span` C++20, is 0.999 with GCC and
  0.992 with Clang (the extremes 0.90x and 1.05x); against `boost::span` at
  C++20 it is 1.002 and 1.000; lumex C++11 against `boost::span` C++11 it is
  1.002 and 0.999.
- **Differences between the C++11 and C++20 columns belong to the harness, not
  to the span.** A few scenarios are 10 to 25 % slower or faster at C++20 (for
  example `ctor_ptr_size`, `front_back` and `mutable_to_const` with GCC), and
  all three implementations move together, because the same loop is placed
  differently by the compiler in the C++20 build. The loops of the smallest
  scenarios are a few instructions long (a fraction of a nanosecond) and are
  affected by where the code and the stack land: two processes of the same
  executable can differ by a factor of two, which is why the table keeps the
  best pass and marks (dagger) a value whose slowest pass was more than 25 %
  slower.
- **No loss was found, so the code of the span was not changed for speed.**
  The things that could make a span slower were checked in the generated code
  and by the scenarios: it is trivially copyable (passed in registers; the
  `pass_noinline` and `pass_static_noinline` rows are equal to the others),
  every observer is `noexcept`, the iterators are pointers (the three sums are
  equal), a static extent stores no size (`ctor_static_array`,
  `static_first`, `pass_static_noinline`).
- **The compile time is the price.** One translation unit that includes the
  header and uses the whole API costs, over the same unit with a stub instead
  of a span (`-fsyntax-only -O0`): GCC 13.2 at C++11 +121 ms for this library
  and +135 ms for `boost::span`; at C++20 +373 ms for this library, +97 ms for
  `std::span` and +249 ms for `boost::span`; Clang 23.1.0 at C++11 +24 ms
  against +3 ms for Boost, at C++20 +219 ms against +77 ms for `std::span`
  and +73 ms for Boost. `std::span` is almost free where `<vector>` or
  `<array>` is included anyway, because `<span>` shares their headers. This
  header includes what it uses: `<iterator>` and `<stdexcept>` (`at`), and
  from C++20 `<memory>` (`std::to_address`, which the constructors from
  iterators need to behave as the standard says for any contiguous iterator)
  and `<ranges>` (the opt-ins `enable_borrowed_range` and `enable_view`). Up
  to C++17 `<memory>` is not included: `address_of` replaces
  `std::addressof`.

## Caveats

- The numbers are for this machine and these compilers. They are not
  a promise about MSVC, other CPUs or other versions: run the benchmark there
  (`Running`).
- Boost 1.92.0 was built with the system GCC 8, but only its headers are used
  (`boost/core/span.hpp`), so the Boost libraries and their compiler do not
  matter. `boost::span` has no `as_bytes` before C++17, so `bytes_sum` has no
  `boost::span C++11` entry.
- The scenarios measure a span in a loop with the optimizer held back by empty
  `asm` statements. They show the cost of the interface, not of a whole
  program, and a fraction of a nanosecond is a handful of instructions.
- `std::span` is measured at C++20 only (where it exists); `boost::span` and
  this library at C++11 and C++20.
- The compile times are the median of 7 runs of one unit on a machine that was
  idle, and differ by 5 to 10 % from run to run.
