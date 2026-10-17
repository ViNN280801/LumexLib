# Atomic shared pointer benchmark results

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic` (Linux-6.1.170-1-generic-x86_64-with-AstraLinux-1.7_x86-64-1.7_x86-64); GCC 13.2 with libstdc++ 13, release build; 24 repetitions, 3 s pauses, 100 ms windows; measured 2026-10-15 in 6.7 minutes, load average 10.03 8.66 6.90 at the start and 10.03 9.74 8.04 at the end.

| Series | Selected implementation | Language mode | is_lock_free |
| --- | --- | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | `split_count_std_wait` | 202002 | yes |
| LumexLib split-count r4 (own ptr), C++20 | `split_count_r4_std_wait` | 202002 | yes |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | `split_count_cas8_std_wait` | 202002 | yes |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 34.76 | 69.71 | 140.82 | 220.20 | 263.41 | 303.87 |
| LumexLib split-count r4 (own ptr), C++20 | 34.64 | 70.22 | 141.62 | 222.44 | 258.16 | 309.85 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 34.65 | 69.47 | 141.88 | 221.45 | 260.53 | 307.25 |
| LumexLib split-count r8 (own ptr), C++20 / mean | 1.00 | 1.00 | 1.00 | 0.99 | 1.01 | 0.99 |
| LumexLib split-count r4 (own ptr), C++20 / mean | 1.00 | 1.01 | 1.00 | 1.00 | 0.99 | 1.01 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 / mean | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |

## `load()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 6.5 (5.4-6.9) | 6.2 (5.1-6.6) | 6.5 (6.1-6.8) | 7.0 (6.5-7.4) | 8.8 (7.5-9.3) | 9.6 (8.4-10.2) |
| LumexLib split-count r4 (own ptr), C++20 | 6.5 (5.7-7.0) | 6.4 (5.9-6.7) | 6.5 (6.0-6.8) | 7.0 (6.6-7.4) | 8.6 (7.8-9.1) | 9.7 (8.2-10.0) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 6.2 (5.4-6.9) | 6.4 (5.9-6.7) | 6.6 (5.6-6.9) | 7.2 (6.6-7.7) | 8.5 (7.9-9.4) | 9.7 (8.2-10.2) |

## `store()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 2.9 (2.6-3.4) | 3.4 (2.5-3.9) | 3.6 (3.3-3.9) | 3.6 (2.9-3.8) | 3.8 (3.3-4.0) | 3.5 (3.2-3.9) |
| LumexLib split-count r4 (own ptr), C++20 | 3.2 (2.8-3.5) | 3.5 (3.1-3.8) | 3.6 (3.1-3.8) | 3.7 (3.3-3.9) | 3.6 (3.2-3.8) | 3.7 (3.3-3.8) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 2.9 (2.5-3.3) | 3.5 (3.3-3.8) | 3.7 (2.6-3.9) | 3.7 (3.3-3.9) | 3.7 (3.5-4.0) | 3.7 (3.2-3.9) |

## `exchange()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 2.6 (2.3-3.1) | 3.6 (2.6-3.8) | 3.8 (3.4-4.0) | 3.8 (2.8-3.9) | 3.9 (3.3-4.2) | 3.6 (3.3-4.0) |
| LumexLib split-count r4 (own ptr), C++20 | 2.9 (2.6-3.1) | 3.5 (2.7-3.8) | 3.8 (3.2-3.9) | 3.8 (3.4-4.0) | 3.8 (3.3-4.0) | 3.7 (3.3-3.9) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 3.0 (2.5-3.2) | 3.6 (2.5-3.8) | 3.9 (2.5-4.0) | 3.7 (3.3-3.9) | 3.8 (3.5-4.1) | 3.8 (3.2-3.9) |

## `compare_exchange_strong()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 13.2 (11.9-13.8) | 15.8 (14.4-16.7) | 16.7 (15.8-17.5) | 18.0 (17.2-18.8) | 21.6 (19.4-22.7) | 23.2 (20.8-24.1) |
| LumexLib split-count r4 (own ptr), C++20 | 13.6 (12.7-14.3) | 15.7 (14.7-16.3) | 16.9 (16.3-17.3) | 18.3 (17.1-18.8) | 21.7 (20.0-22.6) | 22.7 (20.6-23.9) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 12.5 (11.7-14.1) | 14.2 (13.1-15.6) | 15.2 (14.1-16.0) | 16.8 (15.7-17.8) | 20.8 (19.1-21.8) | 22.5 (19.5-23.4) |

Share of successful compare-exchanges:

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 0.62 | 0.33 | 0.21 | 0.20 | 0.19 | 0.15 |
| LumexLib split-count r4 (own ptr), C++20 | 0.63 | 0.34 | 0.20 | 0.20 | 0.18 | 0.15 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 0.62 | 0.37 | 0.28 | 0.28 | 0.27 | 0.23 |

## `load_one_writer()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 6.4 (6.1-6.8) | 5.8 (5.2-6.2) | 6.1 (5.8-6.4) | 6.7 (6.1-6.9) | 8.1 (7.3-8.7) | 9.3 (7.9-9.7) |
| LumexLib split-count r4 (own ptr), C++20 | 6.2 (5.7-6.8) | 5.7 (5.5-6.0) | 6.2 (5.9-6.4) | 6.9 (6.3-7.2) | 8.3 (7.4-8.7) | 9.2 (8.0-9.6) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 6.3 (5.9-6.9) | 5.7 (5.2-6.4) | 6.1 (5.6-6.3) | 6.7 (6.1-7.2) | 8.3 (7.5-9.1) | 9.2 (8.0-9.8) |

## `one_writer_stores()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 4.2 (3.8-4.3) | 4.1 (3.7-4.3) | 4.2 (4.0-4.5) | 7.3 (3.0-16.6) | 7.4 (5.2-20.7) | 7.1 (6.5-43.3) |
| LumexLib split-count r4 (own ptr), C++20 | 4.1 (3.9-4.7) | 4.0 (3.8-4.3) | 4.0 (3.9-4.1) | 3.5 (3.2-8.9) | 7.4 (5.7-8.8) | 6.7 (5.9-8.2) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 4.2 (3.8-4.5) | 4.0 (3.7-4.3) | 4.0 (3.8-4.2) | 5.9 (3.0-30.0) | 7.5 (5.4-20.7) | 6.5 (5.6-8.7) |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.
The last two columns are the median cycles and instructions per operation of the thread in user mode (`perf_event_open`; 0 where the counter was not available).

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS | cycles | instructions |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `uint64_cas` | LumexLib split-count r8 (own ptr), C++20 | 7.8 | 7.8-7.8 | 1.00  33.3 | 13.0 |
| `uint64_cas` | LumexLib split-count r4 (own ptr), C++20 | 7.8 | 7.8-7.8 | 1.00  33.2 | 13.0 |
| `uint64_cas` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 7.8 | 7.8-7.8 | 1.00  33.2 | 13.0 |
| `load` | LumexLib split-count r8 (own ptr), C++20 | 29.6 | 29.5-29.6 | 3.81  126.6 | 118.0 |
| `load` | LumexLib split-count r4 (own ptr), C++20 | 29.7 | 29.6-29.8 | 3.82  126.7 | 118.0 |
| `load` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 29.6 | 29.5-29.7 | 3.81  126.6 | 118.0 |
| `store` | LumexLib split-count r8 (own ptr), C++20 | 18.7 | 18.7-18.8 | 2.40  80.0 | 69.0 |
| `store` | LumexLib split-count r4 (own ptr), C++20 | 18.8 | 18.7-18.8 | 2.41  80.0 | 69.0 |
| `store` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 18.7 | 18.7-18.8 | 2.41  80.0 | 69.0 |
| `exchange` | LumexLib split-count r8 (own ptr), C++20 | 19.3 | 19.3-19.4 | 2.49  82.7 | 81.0 |
| `exchange` | LumexLib split-count r4 (own ptr), C++20 | 19.3 | 19.3-19.4 | 2.49  82.7 | 81.0 |
| `exchange` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 19.5 | 19.4-19.5 | 2.50  83.0 | 82.0 |
| `compare_exchange_strong` | LumexLib split-count r8 (own ptr), C++20 | 61.2 | 61.1-61.5 | 7.87  261.6 | 261.5 |
| `compare_exchange_strong` | LumexLib split-count r4 (own ptr), C++20 | 61.2 | 61.0-61.3 | 7.87  261.6 | 261.5 |
| `compare_exchange_strong` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 61.0 | 60.9-61.2 | 7.84  260.6 | 270.5 |
| `load_one_writer` | LumexLib split-count r8 (own ptr), C++20 | 29.6 | 29.5-29.7 | 3.81  126.5 | 118.0 |
| `load_one_writer` | LumexLib split-count r4 (own ptr), C++20 | 29.6 | 29.5-29.6 | 3.81  126.6 | 118.0 |
| `load_one_writer` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 29.6 | 29.5-29.8 | 3.81  126.5 | 118.0 |
| `one_writer_stores` | LumexLib split-count r8 (own ptr), C++20 | 18.7 | 18.7-18.8 | 2.41  0.0 | 0.0 |
| `one_writer_stores` | LumexLib split-count r4 (own ptr), C++20 | 18.7 | 18.7-18.8 | 2.41  0.0 | 0.0 |
| `one_writer_stores` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 18.7 | 18.6-18.8 | 2.40  0.0 | 0.0 |
| `load_store` | LumexLib split-count r8 (own ptr), C++20 | 48.4 | 48.3-48.6 | 6.22  207.0 | 189.0 |
| `load_store` | LumexLib split-count r4 (own ptr), C++20 | 48.4 | 48.3-48.5 | 6.24  207.0 | 189.0 |
| `load_store` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 48.1 | 48.0-48.2 | 6.19  205.6 | 184.0 |

## Read-heavy: one writer, the others load

One thread stores alternating values in a loop, the other threads `load ()`. Millions of operations per second, from the median time per operation (all readers together for loads; the writer for stores). At 1 thread there is no contention: the thread alone loads (first table) or stores (second table). Higher is better.

Loads per second of the readers, millions

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 4.46 | 7.19 | 7.93 | 7.63 | 7.37 | 7.04 |
| LumexLib split-count r4 (own ptr), C++20 | 4.56 | 7.29 | 7.88 | 7.29 | 7.17 | 7.01 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 4.56 | 7.31 | 7.97 | 7.47 | 7.19 | 7.14 |

Stores per second of the writer, millions

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 7.05 | 3.56 | 1.68 | 0.65 | 0.54 | 0.47 |
| LumexLib split-count r4 (own ptr), C++20 | 7.00 | 3.62 | 1.74 | 1.28 | 0.51 | 0.48 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 7.02 | 3.59 | 1.77 | 0.75 | 0.54 | 0.48 |

