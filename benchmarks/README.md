# Benchmarks of LumexLib

One directory per module: the sources, the `CMakeLists.txt`, a `README.md` with
the method and the recorded numbers, `plot_results.py`, `run_benchmark.py` and
the recorded `results/` (CSV, Markdown tables, SVG charts). Only a Release build
gives numbers that mean anything.

## In the installed package

The package carries the **sources** of the benchmarks, not their binaries:

```text
<prefix>/share/benchmarks/
    <module>/CMakeLists.txt   a project of its own
    <module>/bench_*.cpp      the programs (and the headers they share)
    <module>/README.md        the method and the recorded results
    <module>/plot_results.py  the Markdown tables and charts from a CSV
    <module>/run_benchmark.py the runs of one or more build trees (standard library only)
    <module>/results/         what the maintainers recorded
```

A directory is there when its module was built (`transport` also needs the
dialogue engine and the testing doubles). Each one builds against the installed
package:

```sh
cmake -S <prefix>/share/benchmarks/span -B build -DLumexLib_DIR=<prefix>/lib/cmake/LumexLib
cmake --build build
python3 <prefix>/share/benchmarks/span/run_benchmark.py --build-dir this=build
```

`LumexLib_DIR` is the directory with `LumexLibConfig.cmake`. A directory
configures as a Release build unless `CMAKE_BUILD_TYPE` says otherwise. The
programs are in `build/bin`, which is where `run_benchmark.py` looks for them;
the targets named `Lumex<Module>BenchmarkRun` run a directory's own sweep and
write the results into the build tree, never into the installed sources.

What a consumer brings, and what the benchmarks leave out when it is missing:

- Boost headers (`-DLUMEX_BENCH_BOOST_INCLUDE_DIR=<dir>` or `find_package
  (Boost)`): the comparison with the Boost class of the module. The variants
  that need it are skipped, with a message, without it.
- A compiler with C++20 or C++23: the variants of those standards are skipped,
  with a message, on a compiler that lacks them (GCC 8.3 has no C++23).
- `hazard_pointer`: `-DLUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR` and
  `-DLUMEX_HAZARD_POINTER_BENCH_LLVM_DIR` add the series of two other
  implementations.

The compile time scripts (`compile_time.cpp`, `compile_time.py`) compile with
`-I<checkout>` and, for the protocol, read the tests of the checkout, so they
are not installed; `run_benchmark.py --compile-time` needs a checkout of the
sources, and `protocol` (which has no program, only that script) is installed as
its README and its recorded results.

On Windows the programs load the shared libraries of the package: put its `bin`
directory on `PATH` when running them.
