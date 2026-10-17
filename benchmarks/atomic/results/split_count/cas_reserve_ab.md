# Atomic shared pointer benchmark results

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic` (Linux-6.1.170-1-generic-x86_64-with-AstraLinux-1.7_x86-64-1.7_x86-64); GCC 13.2 with libstdc++ 13, release build; 40 repetitions, 30 s pauses, 100 ms windows; measured 2026-10-15 in 30.0 minutes, load average 5.52 7.36 6.29 at the start and 6.69 5.70 5.48 at the end.

| Series | Selected implementation | Language mode | is_lock_free |
| --- | --- | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | `split_count_std_wait` | 202002 | yes |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | `split_count_cas8_std_wait` | 202002 | yes |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 6.88 | 30.83 | 64.95 | 101.26 | 137.59 | 177.56 | 210.75 | 231.95 | 259.48 | 276.16 | 299.68 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 6.91 | 31.32 | 65.20 | 100.51 | 137.36 | 180.28 | 214.07 | 236.89 | 263.07 | 274.76 | 302.26 |
| LumexLib split-count r8 (own ptr), C++20 / mean | 1.00 | 0.99 | 1.00 | 1.00 | 1.00 | 0.99 | 0.99 | 0.99 | 0.99 | 1.00 | 1.00 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 / mean | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 | 1.01 | 1.01 | 1.01 | 1.01 | 1.00 | 1.00 |

## `load()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 3.8 (3.8-3.8) | 6.8 (6.4-7.5) | 6.5 (6.0-7.0) | 6.7 (6.0-7.1) | 6.8 (6.5-7.2) | 7.0 (6.1-7.3) | 7.3 (6.9-7.6) | 7.7 (7.2-8.1) | 8.3 (7.8-8.9) | 9.0 (8.3-9.8) | 9.1 (8.4-10.1) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 3.8 (3.8-3.8) | 7.1 (6.4-7.6) | 6.6 (6.2-7.2) | 6.7 (6.0-7.2) | 6.6 (5.7-6.9) | 7.1 (6.8-7.4) | 7.2 (6.8-7.6) | 7.8 (7.3-8.3) | 8.3 (7.6-8.9) | 8.9 (8.3-9.7) | 9.1 (8.4-10.4) |

## `store()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 2.4 (2.4-2.4) | 3.2 (3.0-3.6) | 3.6 (3.1-3.8) | 3.7 (3.1-3.9) | 3.8 (3.5-4.1) | 3.7 (3.1-3.9) | 3.7 (3.3-3.9) | 3.6 (3.1-3.9) | 3.7 (3.4-4.0) | 3.6 (3.3-3.9) | 3.5 (3.2-3.8) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 2.4 (2.4-2.4) | 3.0 (2.7-3.5) | 3.6 (3.2-3.9) | 3.7 (2.8-4.0) | 3.6 (2.8-3.8) | 3.8 (3.4-4.1) | 3.6 (3.4-3.9) | 3.7 (3.3-3.9) | 3.6 (3.3-4.0) | 3.6 (3.3-3.8) | 3.5 (3.3-3.8) |

## `exchange()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 2.5 (2.5-2.5) | 2.9 (2.6-3.1) | 3.6 (2.9-3.9) | 3.9 (3.5-4.2) | 3.9 (3.7-4.3) | 4.0 (3.7-4.2) | 3.9 (3.3-4.1) | 3.8 (3.1-4.0) | 3.7 (3.4-4.2) | 3.7 (3.4-4.0) | 3.6 (3.2-3.9) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 2.5 (2.5-2.5) | 2.8 (2.6-3.5) | 3.6 (3.2-3.9) | 3.7 (2.6-4.1) | 3.8 (2.6-4.1) | 3.9 (3.5-4.1) | 3.7 (3.5-4.1) | 3.8 (3.4-4.2) | 3.6 (3.5-4.0) | 3.6 (3.0-3.9) | 3.5 (3.2-3.9) |

## `compare_exchange_strong()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 7.9 (7.8-8.0) | 14.2 (13.1-15.4) | 16.2 (14.8-16.8) | 16.8 (16.1-18.1) | 17.4 (16.1-18.9) | 17.7 (17.0-18.6) | 18.7 (18.1-19.4) | 19.7 (18.8-20.8) | 20.3 (19.6-22.4) | 21.6 (20.8-23.9) | 22.1 (20.8-23.8) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 7.8 (7.8-8.0) | 13.4 (12.5-14.5) | 15.2 (14.6-15.8) | 15.2 (14.0-16.0) | 15.6 (14.8-16.7) | 16.9 (16.0-17.5) | 17.2 (16.4-18.0) | 18.6 (17.1-19.8) | 19.9 (18.5-21.3) | 20.8 (19.5-23.0) | 21.1 (19.8-23.9) |

Share of successful compare-exchanges:

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 1.00 | 0.61 | 0.34 | 0.24 | 0.20 | 0.19 | 0.19 | 0.19 | 0.18 | 0.16 | 0.15 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 1.00 | 0.63 | 0.36 | 0.30 | 0.28 | 0.27 | 0.27 | 0.28 | 0.26 | 0.25 | 0.23 |

## `load_one_writer()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 3.8 (3.8-3.8) | 6.6 (6.3-7.6) | 6.1 (5.7-6.5) | 6.1 (5.8-6.5) | 6.3 (5.8-6.9) | 6.6 (6.2-7.0) | 7.0 (6.7-7.3) | 7.3 (6.8-7.9) | 7.9 (7.5-8.5) | 8.5 (7.9-9.3) | 8.9 (8.0-9.6) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 3.8 (3.8-3.8) | 6.7 (6.1-7.1) | 6.1 (5.9-6.5) | 5.9 (5.6-6.3) | 6.0 (5.7-6.5) | 6.6 (6.3-7.0) | 6.9 (6.4-7.2) | 7.4 (7.0-7.8) | 7.9 (7.4-8.6) | 8.6 (7.9-9.3) | 8.7 (8.2-9.9) |

## `one_writer_stores()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 2.4 (2.4-2.4) | 4.3 (3.8-4.5) | 4.0 (3.8-4.5) | 4.1 (4.0-4.4) | 4.2 (3.9-4.6) | 3.8 (3.4-4.2) | 4.3 (3.1-9.1) | 6.2 (3.3-15.9) | 6.6 (5.3-8.2) | 6.9 (5.7-8.0) | 7.1 (6.1-10.3) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 2.4 (2.4-2.4) | 4.3 (3.9-4.6) | 4.2 (3.8-4.5) | 4.0 (3.7-4.3) | 4.1 (3.9-4.4) | 3.8 (3.5-8.2) | 6.0 (3.2-9.5) | 5.7 (3.0-9.0) | 7.5 (5.0-12.9) | 7.0 (5.2-10.3) | 6.9 (5.6-7.9) |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.
The last two columns are the median cycles and instructions per operation of the thread in user mode (`perf_event_open`; 0 where the counter was not available).

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS | cycles | instructions |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `uint64_cas` | LumexLib split-count r8 (own ptr), C++20 | 6.9 | 6.8-7.5 | 1.00  33.2 | 13.0 |
| `uint64_cas` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 6.9 | 6.8-7.7 | 1.00  33.2 | 13.0 |
| `load` | LumexLib split-count r8 (own ptr), C++20 | 26.4 | 25.9-28.8 | 3.80  126.2 | 118.0 |
| `load` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 26.3 | 25.9-29.4 | 3.80  126.2 | 118.0 |
| `store` | LumexLib split-count r8 (own ptr), C++20 | 16.6 | 16.4-18.7 | 2.41  79.7 | 69.0 |
| `store` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 17.0 | 16.4-18.7 | 2.41  79.8 | 69.0 |
| `exchange` | LumexLib split-count r8 (own ptr), C++20 | 17.1 | 16.9-19.2 | 2.48  82.5 | 81.0 |
| `exchange` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 17.7 | 17.0-19.3 | 2.50  82.8 | 82.0 |
| `compare_exchange_strong` | LumexLib split-count r8 (own ptr), C++20 | 54.3 | 53.5-61.0 | 7.86  260.7 | 261.5 |
| `compare_exchange_strong` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 54.3 | 53.6-60.8 | 7.83  259.7 | 270.5 |
| `load_one_writer` | LumexLib split-count r8 (own ptr), C++20 | 26.3 | 25.9-29.6 | 3.80  126.2 | 118.0 |
| `load_one_writer` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 26.3 | 26.0-29.5 | 3.81  126.1 | 118.0 |
| `one_writer_stores` | LumexLib split-count r8 (own ptr), C++20 | 16.6 | 16.4-18.5 | 2.41  0.0 | 0.0 |
| `one_writer_stores` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 17.0 | 16.4-18.7 | 2.42  0.0 | 0.0 |
| `load_store` | LumexLib split-count r8 (own ptr), C++20 | 43.2 | 42.3-48.3 | 6.22  206.4 | 189.0 |
| `load_store` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 42.9 | 42.0-47.8 | 6.19  204.4 | 184.0 |

## Read-heavy: one writer, the others load

One thread stores alternating values in a loop, the other threads `load ()`. Millions of operations per second, from the median time per operation (all readers together for loads; the writer for stores). At 1 thread there is no contention: the thread alone loads (first table) or stores (second table). Higher is better.

Loads per second of the readers, millions

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 38.39 | 4.87 | 7.53 | 8.10 | 8.18 | 7.81 | 7.46 | 7.55 | 7.37 | 7.26 | 7.10 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 37.95 | 4.78 | 7.60 | 8.38 | 8.35 | 7.57 | 7.51 | 7.31 | 7.42 | 7.20 | 7.08 |

Stores per second of the writer, millions

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 60.43 | 7.65 | 3.72 | 2.47 | 1.76 | 1.52 | 1.18 | 0.70 | 0.55 | 0.52 | 0.48 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 60.09 | 7.41 | 3.73 | 2.54 | 1.80 | 1.51 | 0.76 | 0.76 | 0.54 | 0.48 | 0.51 |

