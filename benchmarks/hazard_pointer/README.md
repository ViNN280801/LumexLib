# Hazard pointer benchmarks

What the hazard pointer module of LumexLib costs, measured with the method of `benchmarks/atomic`: every number is a ratio to a `std::atomic<std::uint64_t>` baseline (a relaxed load followed by a `compare_exchange_strong` on one shared word) measured in the same process, with the same threads and right before the series it normalizes.

## What is measured

| Operation | What one operation is |
| --- | --- |
| `make_destroy` | `make_hazard_pointer ()` and the destruction of the holder (a cache hit takes no atomic operation) |
| `protect_reset` | `protect ()` of a shared pointer with a pre-made holder, a read of the object, `reset_protection ()` |
| `protect_kept` | the same without the reset (the next `protect` ends the previous protection) |
| `make_protect_destroy` | make, protect and destroy: what a function pays that uses one holder per call |
| `retire` | `new` of a small object and `retire ()`, including the reclamation passes that `retire` runs inline and the deleter (`delete`); `new_delete` is the allocator alone, for subtraction |
| `read` | a read of a read-mostly pointer, against the alternatives below |
| `readers_writer` | the same while one thread replaces the pointer at full speed; the time is per read |
| `list_walk` | one node of a 64-node list, hand-over-hand with two holders (the single-writer list of P2530R3) against a plain walk |
| `retire_p50`, `p99`, `p999`, `max` | the latency of one `retire ()` call, one thread, in nanoseconds: the passes show up in the tail |

| Series | What it is |
| --- | --- |
| LumexLib hazard_pointer | the module (`lumex::core::hazard_pointer`, seq_cst fences on both sides) |
| plain acquire load | the lower bound of a read: no protection, unsafe for a pointer that is replaced and freed |
| `std::shared_mutex` read lock | `lock_shared ()` / `unlock_shared ()` around the read |
| `std::atomic_load` of a `shared_ptr` | copy and release of a `std::shared_ptr` |
| memory | `new` and `delete` of a small object |
| libcds HP, libc++ pull request engine | only when the build points at a local copy (below) |

Not measured: the asymmetric barrier (a later phase of the module), Folly (it does not build offline here), a `std::` series (no standard library ships `<hazard_pointer>` yet).

## Method

- **Contention**: 1 thread (uncontended) and every even count up to the logical CPUs, all on the same shared object.
- **One measurement**: the threads start together, run the operation for 100 ms and stop together; the result is the wall time per operation and thread.
- **Normalization**: each block starts with the baseline loop with the same threads; each operation of the block is divided by it, which makes series from different moments comparable.
- **Interleaving and repetitions**: the series take turns, starting with a different one in every repetition; the tables give the median of the per-run ratios with the 25th and 75th percentiles (`run_benchmark.py`).
- **Validity**: a block whose baseline measured nothing fails the run.

## Running

```sh
cmake -S . -B build-bench -DCMAKE_BUILD_TYPE=Release -DLUMEX_BUILD_BENCHMARKS=ON
cmake --build build-bench --target LumexHazardPointerBenchmark
python benchmarks/hazard_pointer/run_benchmark.py --exe build-bench/bin/LumexHazardPointerBenchmark --quick --out-dir build-bench/hazard_pointer-quick
```

`--quick` (3 runs, no pause, 20 ms windows) checks the setup; its numbers are not results, so it writes into the build directory. Without it the sweep uses 100 runs, 30 s pauses and 100 ms windows (about two hours on 20 hardware threads) and rewrites `results/`; the target `LumexHazardPointerBenchmarkRun` does that. `--threads 1,2,4,8` and `--repetitions N` narrow it.

Comparison with other engines needs sources that are not part of this repository:

```sh
cmake ... -DLUMEX_HAZARD_POINTER_BENCH_LIBCDS_DIR=<libcds checkout> -DLUMEX_HAZARD_POINTER_BENCH_LLVM_DIR=<dir with hazard_pointer_port.hpp>
```

`adapter_libcds.hpp` wraps `cds::gc::HP` (guards, `retire`, thread attach); `adapter_llvm.hpp` expects a port of the engine of llvm-project pull request 218218 to the namespace `llvm_port` (header `hazard_pointer_port.hpp`). Neither is part of this repository and neither adapter has been compiled against a real copy yet.

## Results of this directory

`results/` holds a reduced run, not the full sweep: 5 repetitions without pauses, 100 ms windows, 1, 2, 4, 8 and 16 threads (about one minute), made on 2026-10-09. The numbers are indicative; the full sweep (`--repetitions 100 --settle-seconds 30`) was not run.

| | |
| --- | --- |
| CPU | 12th Gen Intel Core i7-12700K, 20 logical CPUs, governor `powersave` |
| OS, kernel | Astra Linux 1.7, Linux 6.1.170 |
| Compiler | GCC 13.2, Release, LumexLib's `Portable` level (`-O2 -march=x86-64`), C++17 for the benchmark, the library as C++11 |
| Load | other jobs of the workstation were queued but not running; load average 9.8 at the start and 12.1 at the end (the desktop session and the benchmark's own threads) |

One thread (ns per operation, median of 5): `make_destroy` 3.9, `protect_reset` 5.9 (the plain acquire load is 1.6, a `shared_mutex` read lock 14.8, `std::atomic_load` of a `shared_ptr` 24.7), a hand-over-hand step of the list 6.0 (plain 1.5), `new` + `retire` with the inline passes 45.2 against 8.9 for `new` + `delete`. The latency of one `retire ()` is 30 ns at the median, 51 ns at the 99th percentile and 11 microseconds at the 99.9th: the tail is the reclamation pass that the crossing `retire` runs inline (maximum 31 microseconds in the median run).

Under contention the reader does not share any cache line with the others (each thread has its own slot), so the time per read stays flat while the baseline compare-exchange grows with the thread count: at 16 threads a read costs 0.03 of the baseline CAS, a `shared_mutex` read lock 4.1 and `std::atomic_load` of a `shared_ptr` 5.2. With one writer replacing the pointer at full speed the hazard pointer read is 0.08 of the baseline at 16 threads, `shared_mutex` 3.7 and `shared_ptr` 5.6 (at 2 threads the writer starves the `shared_mutex` readers: 42.7). The cost of the protocol over an unsafe load is the fence of the announcement: 5.9 ns against 1.6 ns on this machine; an asymmetric barrier (a later phase of the module) would take most of it away.
