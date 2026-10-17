# Atomic shared pointer benchmark results

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic` (Linux-6.1.170-1-generic-x86_64-with-AstraLinux-1.7_x86-64-1.7_x86-64); GCC 13.2 with libstdc++ 13, release build; 24 repetitions, 3 s pauses, 100 ms windows; measured 2026-10-15 in 6.7 minutes, load average 4.68 5.28 5.34 at the start and 10.03 8.66 6.90 at the end.

| Series | Selected implementation | Language mode | is_lock_free |
| --- | --- | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | `split_count_std_wait` | 202002 | yes |
| LumexLib split-count r4 (own ptr), C++20 | `split_count_r4_std_wait` | 202002 | yes |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | `split_count_cas8_std_wait` | 202002 | yes |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 35.64 | 72.77 | 147.97 | 226.37 | 275.55 | 312.47 |
| LumexLib split-count r4 (own ptr), C++20 | 35.18 | 73.32 | 148.44 | 224.66 | 269.94 | 321.45 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 35.72 | 73.34 | 146.90 | 223.66 | 273.46 | 320.67 |
| LumexLib split-count r8 (own ptr), C++20 / mean | 1.00 | 0.99 | 1.00 | 1.01 | 1.01 | 0.98 |
| LumexLib split-count r4 (own ptr), C++20 / mean | 0.99 | 1.00 | 1.00 | 1.00 | 0.99 | 1.01 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 / mean | 1.01 | 1.00 | 0.99 | 0.99 | 1.00 | 1.01 |

## `load()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 6.4 (6.0-7.0) | 6.1 (5.8-6.3) | 6.2 (5.2-6.7) | 6.7 (6.1-7.4) | 7.9 (7.5-8.8) | 8.6 (8.1-9.6) |
| LumexLib split-count r4 (own ptr), C++20 | 6.7 (6.1-7.1) | 6.2 (6.0-6.6) | 6.4 (6.2-6.7) | 7.0 (6.3-7.5) | 8.2 (7.1-9.0) | 8.6 (8.2-10.0) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 6.4 (5.3-6.9) | 6.1 (5.8-6.7) | 6.3 (5.3-6.7) | 6.9 (6.3-7.2) | 7.8 (7.3-9.0) | 8.6 (8.1-9.9) |

## `store()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 3.0 (2.7-3.3) | 3.4 (3.1-3.6) | 3.4 (2.6-3.6) | 3.4 (2.9-3.8) | 3.3 (3.1-3.5) | 3.3 (3.2-3.6) |
| LumexLib split-count r4 (own ptr), C++20 | 3.0 (2.4-3.3) | 3.6 (2.9-3.7) | 3.6 (3.4-3.8) | 3.5 (3.0-3.7) | 3.4 (3.0-4.1) | 3.3 (3.2-3.8) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 3.0 (2.6-3.4) | 3.4 (3.2-3.8) | 3.5 (2.6-3.7) | 3.4 (2.9-3.7) | 3.4 (3.2-4.0) | 3.5 (3.0-3.7) |

## `exchange()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 2.9 (2.5-3.1) | 3.6 (3.3-3.8) | 3.7 (2.4-3.9) | 3.5 (2.7-4.1) | 3.4 (3.3-3.6) | 3.4 (3.2-3.7) |
| LumexLib split-count r4 (own ptr), C++20 | 2.9 (2.5-3.0) | 3.6 (3.2-3.8) | 3.8 (3.6-4.0) | 3.6 (3.0-3.9) | 3.6 (2.8-4.2) | 3.4 (3.3-4.0) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 2.9 (2.6-3.2) | 3.6 (3.3-3.8) | 3.8 (2.7-4.0) | 3.4 (2.8-3.7) | 3.6 (3.2-3.9) | 3.5 (3.0-3.8) |

## `compare_exchange_strong()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 13.1 (12.1-14.0) | 15.4 (14.8-16.4) | 16.1 (15.6-16.8) | 17.2 (16.3-18.1) | 19.6 (18.7-22.5) | 21.2 (19.6-23.1) |
| LumexLib split-count r4 (own ptr), C++20 | 13.2 (11.8-14.0) | 15.6 (15.0-16.5) | 16.3 (15.4-17.3) | 17.8 (16.9-18.3) | 19.7 (18.9-22.8) | 21.2 (19.9-23.7) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 12.5 (11.9-13.3) | 14.1 (13.6-15.6) | 14.7 (14.2-15.3) | 17.0 (15.6-17.3) | 19.1 (18.2-21.3) | 20.2 (19.2-22.8) |

Share of successful compare-exchanges:

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 0.61 | 0.33 | 0.20 | 0.20 | 0.18 | 0.15 |
| LumexLib split-count r4 (own ptr), C++20 | 0.63 | 0.33 | 0.20 | 0.20 | 0.18 | 0.15 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 0.62 | 0.36 | 0.28 | 0.28 | 0.27 | 0.23 |

## `load_one_writer()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 6.2 (5.5-6.6) | 5.9 (5.5-6.3) | 5.8 (5.6-6.1) | 6.1 (5.8-6.6) | 7.3 (7.0-8.5) | 8.2 (7.8-9.3) |
| LumexLib split-count r4 (own ptr), C++20 | 6.5 (5.9-7.0) | 5.8 (5.5-6.2) | 5.8 (5.4-6.2) | 6.5 (6.1-6.9) | 7.8 (7.2-8.7) | 8.2 (7.8-9.4) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 6.2 (5.6-6.7) | 5.8 (5.6-6.3) | 5.8 (5.6-6.1) | 6.6 (6.1-7.0) | 7.8 (7.2-8.6) | 8.2 (7.8-9.5) |

## `one_writer_stores()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 4.1 (3.9-4.5) | 4.0 (3.7-4.2) | 4.0 (3.8-4.2) | 9.1 (3.5-25.8) | 7.7 (6.4-14.9) | 6.7 (5.8-15.3) |
| LumexLib split-count r4 (own ptr), C++20 | 4.1 (3.8-4.5) | 4.0 (3.7-4.2) | 4.0 (3.8-4.4) | 5.8 (3.0-11.9) | 7.0 (5.0-9.0) | 7.2 (5.9-14.5) |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 4.1 (3.8-4.5) | 4.0 (3.9-4.2) | 3.8 (3.8-4.2) | 3.7 (3.2-8.7) | 5.4 (3.7-7.9) | 6.4 (4.8-9.2) |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.
The last two columns are the median cycles and instructions per operation of the thread in user mode (`perf_event_open`; 0 where the counter was not available).

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS | cycles | instructions |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `uint64_cas` | LumexLib split-count r8 (own ptr), C++20 | 7.8 | 7.7-7.8 | 1.00  33.2 | 13.0 |
| `uint64_cas` | LumexLib split-count r4 (own ptr), C++20 | 7.8 | 7.8-7.8 | 1.00  33.3 | 13.0 |
| `uint64_cas` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 7.8 | 7.8-7.8 | 1.00  33.2 | 13.0 |
| `load` | LumexLib split-count r8 (own ptr), C++20 | 29.6 | 29.5-29.7 | 3.81  126.6 | 118.0 |
| `load` | LumexLib split-count r4 (own ptr), C++20 | 29.6 | 29.5-29.7 | 3.82  126.5 | 118.0 |
| `load` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 29.6 | 29.5-29.7 | 3.81  126.6 | 118.0 |
| `store` | LumexLib split-count r8 (own ptr), C++20 | 18.7 | 18.7-18.8 | 2.41  80.0 | 69.0 |
| `store` | LumexLib split-count r4 (own ptr), C++20 | 18.8 | 18.7-18.8 | 2.42  80.0 | 69.0 |
| `store` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 18.7 | 18.6-18.8 | 2.41  80.1 | 69.0 |
| `exchange` | LumexLib split-count r8 (own ptr), C++20 | 19.3 | 19.3-19.4 | 2.49  82.8 | 81.0 |
| `exchange` | LumexLib split-count r4 (own ptr), C++20 | 19.3 | 19.3-19.4 | 2.49  82.7 | 81.0 |
| `exchange` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 19.4 | 19.4-19.5 | 2.50  83.1 | 82.0 |
| `compare_exchange_strong` | LumexLib split-count r8 (own ptr), C++20 | 61.2 | 61.0-61.6 | 7.88  261.8 | 261.5 |
| `compare_exchange_strong` | LumexLib split-count r4 (own ptr), C++20 | 61.2 | 60.9-61.3 | 7.88  261.2 | 261.5 |
| `compare_exchange_strong` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 61.0 | 60.8-61.1 | 7.84  260.5 | 270.5 |
| `load_one_writer` | LumexLib split-count r8 (own ptr), C++20 | 29.6 | 29.5-29.7 | 3.81  126.6 | 118.0 |
| `load_one_writer` | LumexLib split-count r4 (own ptr), C++20 | 29.6 | 29.5-29.8 | 3.82  126.6 | 118.0 |
| `load_one_writer` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 29.6 | 29.5-29.7 | 3.81  126.6 | 118.0 |
| `one_writer_stores` | LumexLib split-count r8 (own ptr), C++20 | 18.7 | 18.7-18.8 | 2.41  0.0 | 0.0 |
| `one_writer_stores` | LumexLib split-count r4 (own ptr), C++20 | 18.7 | 18.7-18.8 | 2.41  0.0 | 0.0 |
| `one_writer_stores` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 18.7 | 18.7-18.8 | 2.41  0.0 | 0.0 |
| `load_store` | LumexLib split-count r8 (own ptr), C++20 | 48.3 | 48.3-48.4 | 6.23  206.9 | 189.0 |
| `load_store` | LumexLib split-count r4 (own ptr), C++20 | 48.4 | 48.2-48.5 | 6.22  206.8 | 189.0 |
| `load_store` | LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 47.9 | 47.8-48.1 | 6.17  205.2 | 184.0 |

## Read-heavy: one writer, the others load

One thread stores alternating values in a loop, the other threads `load ()`. Millions of operations per second, from the median time per operation (all readers together for loads; the writer for stores). At 1 thread there is no contention: the thread alone loads (first table) or stores (second table). Higher is better.

Loads per second of the readers, millions

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 4.69 | 7.18 | 7.97 | 7.76 | 7.48 | 7.11 |
| LumexLib split-count r4 (own ptr), C++20 | 4.51 | 7.16 | 7.92 | 7.49 | 7.19 | 7.21 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 4.49 | 7.15 | 8.02 | 7.25 | 7.28 | 7.18 |

Stores per second of the writer, millions

| Series | 2 | 4 | 8 | 12 | 16 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count r8 (own ptr), C++20 | 6.97 | 3.56 | 1.72 | 0.52 | 0.50 | 0.49 |
| LumexLib split-count r4 (own ptr), C++20 | 6.99 | 3.58 | 1.70 | 0.75 | 0.53 | 0.46 |
| LumexLib split-count r8 + cas reserve 8 (own ptr), C++20 | 6.97 | 3.53 | 1.74 | 1.14 | 0.67 | 0.54 |

