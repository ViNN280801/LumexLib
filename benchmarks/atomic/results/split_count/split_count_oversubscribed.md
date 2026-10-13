# Atomic shared pointer benchmark results

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic` (Linux-6.1.170-1-generic-x86_64-with-AstraLinux-1.7_x86-64-1.7_x86-64); GCC 13.2 with libstdc++ 13, release build; 30 repetitions, 10 s pauses, 100 ms windows; measured 2026-10-13 in 22.3 minutes, load average 10.36 8.91 7.38 at the start and 30.04 26.70 22.12 at the end.

| Series | Selected implementation | Language mode | is_lock_free |
| --- | --- | ---: | ---: |
| LumexLib lock-based, C++20 | `lock_based_std_wait` | 202002 | no |
| LumexLib lock-free, C++20 | `lock_free_std_wait` | 202002 | yes |
| LumexLib lock-free, C++20, deferred | `lock_free_deferred_std_wait` | 202002 | yes |
| LumexLib std-backed, C++20 (wraps std) | `std_backed_std_wait` | 202002 | no |
| std::atomic, libstdc++ 13 | `libstdc++ 13` | 202002 | no |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | `boost 1_92` | 202002 | no |
| LumexLib split-count (own ptr), C++11 | `split_count_table_wait` | 201103 | yes |
| LumexLib lock-based (own ptr), C++11 | `lock_based_lumex_table_wait` | 201103 | no |
| LumexLib split-count r8 (own ptr), C++20 | `split_count_std_wait` | 202002 | yes |
| LumexLib split-count r4 (own ptr), C++20 | `split_count_r4_std_wait` | 202002 | yes |
| LumexLib lock-based (own ptr), C++20 | `lock_based_lumex_std_wait` | 202002 | no |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 309.76 | 595.62 | 890.03 |
| LumexLib lock-free, C++20 | 308.61 | 588.05 | 897.91 |
| LumexLib lock-free, C++20, deferred | 313.50 | 586.61 | 879.19 |
| LumexLib std-backed, C++20 (wraps std) | 306.45 | 595.75 | 889.43 |
| std::atomic, libstdc++ 13 | 304.54 | 584.43 | 896.77 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 306.72 | 594.20 | 898.01 |
| LumexLib split-count (own ptr), C++11 | 354.31 | 605.82 | 881.84 |
| LumexLib lock-based (own ptr), C++11 | 307.54 | 604.27 | 915.53 |
| LumexLib split-count r8 (own ptr), C++20 | 308.88 | 591.18 | 896.15 |
| LumexLib split-count r4 (own ptr), C++20 | 306.68 | 588.93 | 892.87 |
| LumexLib lock-based (own ptr), C++20 | 310.65 | 584.56 | 892.60 |
| LumexLib lock-based, C++20 / mean | 0.99 | 1.00 | 1.00 |
| LumexLib lock-free, C++20 / mean | 0.99 | 0.99 | 1.00 |
| LumexLib lock-free, C++20, deferred / mean | 1.00 | 0.99 | 0.98 |
| LumexLib std-backed, C++20 (wraps std) / mean | 0.98 | 1.01 | 1.00 |
| std::atomic, libstdc++ 13 / mean | 0.97 | 0.99 | 1.00 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 / mean | 0.98 | 1.00 | 1.00 |
| LumexLib split-count (own ptr), C++11 / mean | 1.13 | 1.02 | 0.99 |
| LumexLib lock-based (own ptr), C++11 / mean | 0.98 | 1.02 | 1.02 |
| LumexLib split-count r8 (own ptr), C++20 / mean | 0.99 | 1.00 | 1.00 |
| LumexLib split-count r4 (own ptr), C++20 / mean | 0.98 | 0.99 | 1.00 |
| LumexLib lock-based (own ptr), C++20 / mean | 0.99 | 0.99 | 1.00 |

## `load()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 12.8 (11.7-13.9) | 14.3 (13.0-17.4) | 14.5 (13.5-17.3) |
| LumexLib lock-free, C++20 | 2.2 (2.0-2.5) | 2.2 (2.0-2.4) | 2.2 (2.0-2.4) |
| LumexLib lock-free, C++20, deferred | 2.2 (1.9-2.4) | 2.2 (2.0-2.4) | 2.2 (2.1-2.5) |
| LumexLib std-backed, C++20 (wraps std) | 28.3 (27.2-29.8) | 69.1 (48.7-76.4) | 81.9 (72.7-110.0) |
| std::atomic, libstdc++ 13 | 28.7 (27.3-31.8) | 60.5 (48.4-74.9) | 98.3 (81.4-120.9) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 1.3 (1.2-1.4) | 1.4 (1.4-1.5) | 1.7 (1.6-1.8) |
| LumexLib split-count (own ptr), C++11 | 8.0 (7.3-8.9) | 8.8 (8.5-9.4) | 8.9 (8.3-9.3) |
| LumexLib lock-based (own ptr), C++11 | 7.4 (7.1-7.9) | 7.7 (7.1-8.7) | 7.8 (7.1-8.8) |
| LumexLib split-count r8 (own ptr), C++20 | 9.3 (8.8-9.7) | 9.2 (8.6-9.7) | 9.0 (8.1-9.3) |
| LumexLib split-count r4 (own ptr), C++20 | 9.1 (8.5-9.7) | 8.9 (8.5-9.5) | 8.7 (8.4-9.2) |
| LumexLib lock-based (own ptr), C++20 | 12.1 (10.8-14.8) | 14.7 (13.3-17.1) | 14.3 (12.8-15.3) |

## `store()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 13.3 (12.2-15.1) | 14.0 (13.4-15.8) | 14.6 (13.7-16.0) |
| LumexLib lock-free, C++20 | 4.3 (4.1-4.5) | 4.3 (4.1-4.5) | 4.2 (3.9-4.3) |
| LumexLib lock-free, C++20, deferred | 5.0 (4.7-5.5) | 4.8 (4.4-5.1) | 4.5 (4.3-4.8) |
| LumexLib std-backed, C++20 (wraps std) | 17.9 (16.9-20.4) | 37.9 (31.6-45.1) | 54.3 (46.1-72.1) |
| std::atomic, libstdc++ 13 | 18.4 (17.6-20.4) | 35.4 (29.3-45.1) | 48.2 (39.3-57.5) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 8.3 (5.3-10.4) | 4.6 (4.3-5.0) | 5.5 (5.3-5.8) |
| LumexLib split-count (own ptr), C++11 | 3.0 (2.6-3.1) | 3.2 (2.9-3.5) | 3.2 (3.1-3.4) |
| LumexLib lock-based (own ptr), C++11 | 10.2 (9.6-11.0) | 10.2 (9.8-11.2) | 10.6 (10.0-11.2) |
| LumexLib split-count r8 (own ptr), C++20 | 3.5 (3.3-3.6) | 3.4 (3.1-3.6) | 3.3 (2.9-3.5) |
| LumexLib split-count r4 (own ptr), C++20 | 3.5 (3.2-3.6) | 3.4 (3.0-3.7) | 3.2 (3.0-3.4) |
| LumexLib lock-based (own ptr), C++20 | 12.9 (12.0-14.4) | 14.1 (13.5-15.4) | 14.0 (13.3-15.2) |

## `exchange()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 13.7 (12.5-15.2) | 14.3 (13.5-16.0) | 14.7 (13.7-16.1) |
| LumexLib lock-free, C++20 | 6.3 (6.0-6.9) | 6.3 (6.1-6.9) | 6.2 (5.8-6.5) |
| LumexLib lock-free, C++20, deferred | 6.6 (6.4-7.2) | 6.4 (6.1-6.9) | 6.2 (6.0-6.7) |
| LumexLib std-backed, C++20 (wraps std) | 22.7 (22.1-25.8) | 49.0 (39.2-56.9) | 74.9 (58.3-118.0) |
| std::atomic, libstdc++ 13 | 24.1 (21.6-25.2) | 52.2 (37.9-63.9) | 71.1 (58.7-88.0) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 4.1 (3.6-5.0) | 4.4 (4.3-4.9) | 5.4 (5.1-5.7) |
| LumexLib split-count (own ptr), C++11 | 3.0 (2.7-3.3) | 3.3 (3.0-3.7) | 3.3 (3.2-3.5) |
| LumexLib lock-based (own ptr), C++11 | 10.1 (9.5-10.5) | 10.2 (9.9-11.2) | 10.5 (10.0-11.1) |
| LumexLib split-count r8 (own ptr), C++20 | 3.6 (3.3-3.8) | 3.4 (3.2-3.7) | 3.4 (2.8-3.6) |
| LumexLib split-count r4 (own ptr), C++20 | 3.5 (3.2-3.8) | 3.5 (3.1-3.7) | 3.3 (3.0-3.6) |
| LumexLib lock-based (own ptr), C++20 | 13.2 (12.2-14.5) | 14.3 (13.6-15.7) | 14.7 (13.5-15.7) |

## `compare_exchange_strong()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 32.0 (29.7-36.2) | 33.6 (30.9-39.3) | 34.1 (32.4-38.5) |
| LumexLib lock-free, C++20 | 10.2 (9.1-11.5) | 9.9 (8.7-11.2) | 9.5 (8.3-10.5) |
| LumexLib lock-free, C++20, deferred | 9.1 (8.6-10.8) | 9.2 (8.6-10.5) | 9.1 (8.1-10.6) |
| LumexLib std-backed, C++20 (wraps std) | 64.5 (61.3-71.6) | 130.9 (118.0-154.3) | 188.2 (156.8-235.6) |
| std::atomic, libstdc++ 13 | 65.2 (62.0-71.8) | 127.6 (103.8-157.8) | 194.5 (139.0-255.5) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 4.5 (4.1-5.1) | 5.4 (5.0-5.7) | 6.6 (6.2-7.0) |
| LumexLib split-count (own ptr), C++11 | 19.4 (18.1-21.3) | 21.3 (20.4-22.6) | 21.3 (19.6-22.7) |
| LumexLib lock-based (own ptr), C++11 | 22.5 (21.2-24.1) | 22.4 (21.7-25.0) | 23.1 (22.3-25.3) |
| LumexLib split-count r8 (own ptr), C++20 | 22.2 (21.3-22.9) | 21.7 (20.9-23.0) | 20.9 (19.6-22.1) |
| LumexLib split-count r4 (own ptr), C++20 | 22.2 (21.2-23.5) | 21.9 (20.6-22.9) | 21.1 (19.8-22.1) |
| LumexLib lock-based (own ptr), C++20 | 30.7 (29.2-35.0) | 34.2 (32.4-38.1) | 34.6 (31.3-38.4) |

Share of successful compare-exchanges:

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 0.54 | 0.55 | 0.55 |
| LumexLib lock-free, C++20 | 0.23 | 0.24 | 0.23 |
| LumexLib lock-free, C++20, deferred | 0.24 | 0.24 | 0.24 |
| LumexLib std-backed, C++20 (wraps std) | 0.46 | 0.46 | 0.46 |
| std::atomic, libstdc++ 13 | 0.46 | 0.46 | 0.46 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 0.97 | 0.94 | 0.91 |
| LumexLib split-count (own ptr), C++11 | 0.15 | 0.14 | 0.15 |
| LumexLib lock-based (own ptr), C++11 | 0.62 | 0.63 | 0.63 |
| LumexLib split-count r8 (own ptr), C++20 | 0.15 | 0.14 | 0.15 |
| LumexLib split-count r4 (own ptr), C++20 | 0.15 | 0.14 | 0.15 |
| LumexLib lock-based (own ptr), C++20 | 0.54 | 0.54 | 0.54 |

## `load_one_writer()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 12.2 (11.4-14.2) | 14.3 (13.2-17.4) | 15.0 (13.4-16.7) |
| LumexLib lock-free, C++20 | 2.1 (2.0-2.4) | 2.2 (2.0-2.5) | 2.2 (2.0-2.3) |
| LumexLib lock-free, C++20, deferred | 2.0 (1.9-2.4) | 2.2 (2.0-2.4) | 2.1 (2.0-2.4) |
| LumexLib std-backed, C++20 (wraps std) | 28.8 (27.1-31.2) | 55.1 (46.4-74.8) | 86.8 (74.5-105.1) |
| std::atomic, libstdc++ 13 | 28.2 (27.2-30.0) | 61.9 (50.7-80.0) | 87.2 (67.8-103.3) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 1.3 (1.1-1.7) | 1.4 (1.4-1.6) | 1.6 (1.5-1.8) |
| LumexLib split-count (own ptr), C++11 | 7.7 (6.9-8.5) | 8.6 (8.2-9.3) | 8.7 (8.2-9.3) |
| LumexLib lock-based (own ptr), C++11 | 7.9 (7.5-8.6) | 8.4 (7.5-9.0) | 8.5 (7.7-9.4) |
| LumexLib split-count r8 (own ptr), C++20 | 8.8 (8.4-9.2) | 8.9 (8.5-9.7) | 8.6 (8.0-9.2) |
| LumexLib split-count r4 (own ptr), C++20 | 8.8 (8.3-9.6) | 8.9 (8.4-9.3) | 8.7 (8.2-9.3) |
| LumexLib lock-based (own ptr), C++20 | 11.8 (10.7-13.7) | 14.4 (12.7-16.9) | 14.7 (12.4-16.6) |

## `one_writer_stores()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 14.9 (10.5-18.0) | 13.5 (10.1-19.7) | 14.9 (12.3-17.3) |
| LumexLib lock-free, C++20 | 3.9 (3.2-5.2) | 3.7 (2.9-4.5) | 3.9 (3.0-5.3) |
| LumexLib lock-free, C++20, deferred | 3.7 (3.4-4.5) | 2.7 (2.1-4.4) | 3.9 (3.2-5.6) |
| LumexLib std-backed, C++20 (wraps std) | 31.7 (29.8-39.3) | 58.5 (36.2-75.8) | 108.2 (74.9-145.4) |
| std::atomic, libstdc++ 13 | 33.9 (31.7-52.4) | 54.5 (39.6-86.8) | 95.3 (69.0-171.8) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 20.4 (17.2-25.5) | 17.9 (15.8-21.0) | 18.0 (16.2-20.2) |
| LumexLib split-count (own ptr), C++11 | 6.1 (5.2-7.2) | 4.9 (3.7-8.2) | 6.3 (4.9-8.8) |
| LumexLib lock-based (own ptr), C++11 | 8.7 (8.0-9.3) | 9.6 (9.0-10.6) | 8.9 (7.9-10.3) |
| LumexLib split-count r8 (own ptr), C++20 | 6.6 (6.0-7.4) | 4.5 (3.3-6.5) | 7.1 (5.8-8.5) |
| LumexLib split-count r4 (own ptr), C++20 | 6.6 (5.8-7.0) | 3.9 (3.3-5.9) | 7.1 (6.2-8.9) |
| LumexLib lock-based (own ptr), C++20 | 12.4 (9.5-20.3) | 12.9 (10.8-17.8) | 15.4 (13.4-20.2) |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.
The last two columns are the median cycles and instructions per operation of the thread in user mode (`perf_event_open`; 0 where the counter was not available).

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS | cycles | instructions |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `uint64_cas` | LumexLib lock-based, C++20 | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | LumexLib lock-free, C++20 | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | LumexLib lock-free, C++20, deferred | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | LumexLib std-backed, C++20 (wraps std) | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | std::atomic, libstdc++ 13 | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | LumexLib split-count (own ptr), C++11 | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | LumexLib lock-based (own ptr), C++11 | 7.8 | 7.6-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | LumexLib split-count r8 (own ptr), C++20 | 7.8 | 7.6-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | LumexLib split-count r4 (own ptr), C++20 | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `uint64_cas` | LumexLib lock-based (own ptr), C++20 | 7.8 | 7.7-7.8 | 1.00  33.4 | 13.0 |
| `load` | LumexLib lock-based, C++20 | 18.5 | 18.4-18.6 | 2.37  79.3 | 40.0 |
| `load` | LumexLib lock-free, C++20 | 19.0 | 18.7-19.1 | 2.45  81.6 | 108.0 |
| `load` | LumexLib lock-free, C++20, deferred | 19.1 | 18.9-19.1 | 2.45  81.7 | 108.0 |
| `load` | LumexLib std-backed, C++20 (wraps std) | 18.6 | 18.4-18.7 | 2.38  79.7 | 32.0 |
| `load` | std::atomic, libstdc++ 13 | 18.9 | 18.7-19.1 | 2.42  80.9 | 32.0 |
| `load` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 15.9 | 14.9-16.0 | 2.05  68.3 | 24.0 |
| `load` | LumexLib split-count (own ptr), C++11 | 28.9 | 28.8-29.1 | 3.71  124.0 | 113.0 |
| `load` | LumexLib lock-based (own ptr), C++11 | 19.1 | 19.0-19.2 | 2.45  81.9 | 48.0 |
| `load` | LumexLib split-count r8 (own ptr), C++20 | 29.4 | 29.2-29.5 | 3.77  125.8 | 111.0 |
| `load` | LumexLib split-count r4 (own ptr), C++20 | 29.4 | 29.2-29.4 | 3.76  125.9 | 111.0 |
| `load` | LumexLib lock-based (own ptr), C++20 | 18.0 | 17.8-18.1 | 2.31  77.2 | 29.0 |
| `store` | LumexLib lock-based, C++20 | 21.1 | 20.9-21.2 | 2.71  90.4 | 43.0 |
| `store` | LumexLib lock-free, C++20 | 45.5 | 45.3-45.7 | 5.82  194.9 | 530.5 |
| `store` | LumexLib lock-free, C++20, deferred | 66.8 | 66.4-67.1 | 8.57  285.8 | 340.8 |
| `store` | LumexLib std-backed, C++20 (wraps std) | 20.8 | 20.6-20.8 | 2.66  89.0 | 37.0 |
| `store` | std::atomic, libstdc++ 13 | 20.7 | 20.4-20.8 | 2.65  88.5 | 37.0 |
| `store` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 24.9 | 24.8-25.0 | 3.20  106.7 | 35.0 |
| `store` | LumexLib split-count (own ptr), C++11 | 19.8 | 19.7-19.9 | 2.55  85.0 | 89.0 |
| `store` | LumexLib lock-based (own ptr), C++11 | 22.1 | 21.4-22.1 | 2.83  94.6 | 53.0 |
| `store` | LumexLib split-count r8 (own ptr), C++20 | 18.3 | 18.2-18.4 | 2.35  78.4 | 64.0 |
| `store` | LumexLib split-count r4 (own ptr), C++20 | 18.3 | 18.2-18.4 | 2.34  78.5 | 64.0 |
| `store` | LumexLib lock-based (own ptr), C++20 | 20.8 | 20.6-20.9 | 2.67  89.3 | 34.0 |
| `exchange` | LumexLib lock-based, C++20 | 21.3 | 21.1-21.4 | 2.73  91.3 | 46.0 |
| `exchange` | LumexLib lock-free, C++20 | 56.1 | 55.7-56.4 | 7.21  240.4 | 558.5 |
| `exchange` | LumexLib lock-free, C++20, deferred | 75.9 | 75.5-76.3 | 9.77  325.4 | 367.7 |
| `exchange` | LumexLib std-backed, C++20 (wraps std) | 20.6 | 20.5-20.8 | 2.64  88.6 | 40.0 |
| `exchange` | std::atomic, libstdc++ 13 | 20.8 | 20.4-20.9 | 2.67  89.1 | 40.0 |
| `exchange` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 26.4 | 26.2-26.7 | 3.40  113.3 | 41.0 |
| `exchange` | LumexLib split-count (own ptr), C++11 | 18.3 | 18.2-18.4 | 2.35  78.3 | 98.0 |
| `exchange` | LumexLib lock-based (own ptr), C++11 | 22.0 | 21.6-22.0 | 2.82  94.2 | 57.0 |
| `exchange` | LumexLib split-count r8 (own ptr), C++20 | 19.2 | 19.0-19.3 | 2.46  82.0 | 76.0 |
| `exchange` | LumexLib split-count r4 (own ptr), C++20 | 19.2 | 19.1-19.2 | 2.46  82.1 | 76.0 |
| `exchange` | LumexLib lock-based (own ptr), C++20 | 21.0 | 20.9-21.1 | 2.69  90.1 | 36.0 |
| `compare_exchange_strong` | LumexLib lock-based, C++20 | 47.8 | 47.6-48.1 | 6.13  204.7 | 109.5 |
| `compare_exchange_strong` | LumexLib lock-free, C++20 | 79.1 | 78.6-79.4 | 10.16  338.9 | 809.5 |
| `compare_exchange_strong` | LumexLib lock-free, C++20, deferred | 100.8 | 99.9-101.5 | 12.94  431.8 | 620.2 |
| `compare_exchange_strong` | LumexLib std-backed, C++20 (wraps std) | 60.4 | 59.8-60.7 | 7.74  258.4 | 106.5 |
| `compare_exchange_strong` | std::atomic, libstdc++ 13 | 60.4 | 60.1-60.7 | 7.75  258.8 | 107.5 |
| `compare_exchange_strong` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 47.4 | 46.9-47.8 | 6.10  203.1 | 83.0 |
| `compare_exchange_strong` | LumexLib split-count (own ptr), C++11 | 57.3 | 56.9-57.5 | 7.34  245.8 | 224.0 |
| `compare_exchange_strong` | LumexLib lock-based (own ptr), C++11 | 48.4 | 48.0-48.7 | 6.23  207.5 | 118.5 |
| `compare_exchange_strong` | LumexLib split-count r8 (own ptr), C++20 | 60.6 | 60.2-61.1 | 7.79  260.0 | 240.5 |
| `compare_exchange_strong` | LumexLib split-count r4 (own ptr), C++20 | 60.7 | 60.3-60.9 | 7.76  260.2 | 240.5 |
| `compare_exchange_strong` | LumexLib lock-based (own ptr), C++20 | 48.5 | 48.2-48.7 | 6.21  208.0 | 102.5 |
| `load_one_writer` | LumexLib lock-based, C++20 | 18.5 | 18.4-18.6 | 2.37  79.3 | 40.0 |
| `load_one_writer` | LumexLib lock-free, C++20 | 19.0 | 18.9-19.1 | 2.45  81.7 | 108.0 |
| `load_one_writer` | LumexLib lock-free, C++20, deferred | 19.1 | 18.9-19.2 | 2.46  81.9 | 108.0 |
| `load_one_writer` | LumexLib std-backed, C++20 (wraps std) | 18.6 | 18.2-18.7 | 2.38  79.7 | 32.0 |
| `load_one_writer` | std::atomic, libstdc++ 13 | 18.9 | 18.7-18.9 | 2.42  80.7 | 32.0 |
| `load_one_writer` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 16.0 | 14.9-16.0 | 2.05  68.4 | 24.0 |
| `load_one_writer` | LumexLib split-count (own ptr), C++11 | 28.9 | 28.7-29.1 | 3.72  124.0 | 113.0 |
| `load_one_writer` | LumexLib lock-based (own ptr), C++11 | 19.1 | 18.9-19.2 | 2.45  82.0 | 48.0 |
| `load_one_writer` | LumexLib split-count r8 (own ptr), C++20 | 29.4 | 29.2-29.5 | 3.77  126.1 | 111.0 |
| `load_one_writer` | LumexLib split-count r4 (own ptr), C++20 | 29.4 | 29.3-29.4 | 3.76  125.9 | 111.0 |
| `load_one_writer` | LumexLib lock-based (own ptr), C++20 | 18.0 | 17.7-18.1 | 2.30  77.1 | 29.0 |
| `one_writer_stores` | LumexLib lock-based, C++20 | 21.0 | 20.9-21.3 | 2.70  0.0 | 0.0 |
| `one_writer_stores` | LumexLib lock-free, C++20 | 45.5 | 45.1-45.7 | 5.83  0.0 | 0.0 |
| `one_writer_stores` | LumexLib lock-free, C++20, deferred | 67.0 | 66.5-67.5 | 8.60  0.0 | 0.0 |
| `one_writer_stores` | LumexLib std-backed, C++20 (wraps std) | 20.8 | 20.5-20.9 | 2.67  0.0 | 0.0 |
| `one_writer_stores` | std::atomic, libstdc++ 13 | 20.6 | 20.2-20.7 | 2.65  0.0 | 0.0 |
| `one_writer_stores` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 24.9 | 24.7-25.1 | 3.20  0.0 | 0.0 |
| `one_writer_stores` | LumexLib split-count (own ptr), C++11 | 19.8 | 19.7-19.9 | 2.55  0.0 | 0.0 |
| `one_writer_stores` | LumexLib lock-based (own ptr), C++11 | 22.1 | 21.0-22.2 | 2.84  0.0 | 0.0 |
| `one_writer_stores` | LumexLib split-count r8 (own ptr), C++20 | 18.3 | 18.2-18.4 | 2.35  0.0 | 0.0 |
| `one_writer_stores` | LumexLib split-count r4 (own ptr), C++20 | 18.3 | 18.2-18.3 | 2.35  0.0 | 0.0 |
| `one_writer_stores` | LumexLib lock-based (own ptr), C++20 | 20.8 | 20.6-21.0 | 2.67  0.0 | 0.0 |
| `load_store` | LumexLib lock-based, C++20 | 41.3 | 41.0-41.7 | 5.29  177.0 | 84.0 |
| `load_store` | LumexLib lock-free, C++20 | 62.6 | 62.3-63.0 | 8.03  268.3 | 641.0 |
| `load_store` | LumexLib lock-free, C++20, deferred | 84.1 | 83.3-84.3 | 10.79  360.3 | 449.7 |
| `load_store` | LumexLib std-backed, C++20 (wraps std) | 42.4 | 41.9-42.6 | 5.43  181.8 | 69.0 |
| `load_store` | std::atomic, libstdc++ 13 | 42.3 | 41.7-42.6 | 5.43  181.6 | 69.0 |
| `load_store` | boost::atomic_shared_ptr (spinlock), Boost 1_92 | 41.2 | 40.3-41.5 | 5.31  177.1 | 60.0 |
| `load_store` | LumexLib split-count (own ptr), C++11 | 47.0 | 46.8-47.3 | 6.04  201.7 | 198.0 |
| `load_store` | LumexLib lock-based (own ptr), C++11 | 41.5 | 40.5-41.7 | 5.33  178.1 | 98.0 |
| `load_store` | LumexLib split-count r8 (own ptr), C++20 | 47.8 | 47.5-48.0 | 6.13  204.6 | 177.0 |
| `load_store` | LumexLib split-count r4 (own ptr), C++20 | 47.6 | 47.5-47.8 | 6.09  204.2 | 177.0 |
| `load_store` | LumexLib lock-based (own ptr), C++20 | 39.6 | 39.4-39.8 | 5.08  169.7 | 71.0 |

## Read-heavy: one writer, the others load

One thread stores alternating values in a loop, the other threads `load ()`. Millions of operations per second, from the median time per operation (all readers together for loads; the writer for stores). At 1 thread there is no contention: the thread alone loads (first table) or stores (second table). Higher is better.

Loads per second of the readers, millions

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 5.07 | 4.53 | 4.73 |
| LumexLib lock-free, C++20 | 29.34 | 30.44 | 30.54 |
| LumexLib lock-free, C++20, deferred | 30.64 | 30.65 | 30.90 |
| LumexLib std-backed, C++20 (wraps std) | 2.14 | 1.13 | 0.75 |
| std::atomic, libstdc++ 13 | 2.21 | 1.07 | 0.75 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 52.70 | 43.93 | 40.26 |
| LumexLib split-count (own ptr), C++11 | 7.05 | 7.45 | 7.54 |
| LumexLib lock-based (own ptr), C++11 | 7.78 | 7.58 | 7.61 |
| LumexLib split-count r8 (own ptr), C++20 | 7.16 | 7.42 | 7.62 |
| LumexLib split-count r4 (own ptr), C++20 | 7.05 | 7.46 | 7.62 |
| LumexLib lock-based (own ptr), C++20 | 5.12 | 4.55 | 4.57 |

Stores per second of the writer, millions

| Series | 20 | 40 | 60 |
| --- | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 0.23 | 0.13 | 0.07 |
| LumexLib lock-free, C++20 | 0.91 | 0.47 | 0.27 |
| LumexLib lock-free, C++20, deferred | 0.87 | 0.65 | 0.29 |
| LumexLib std-backed, C++20 (wraps std) | 0.10 | 0.03 | 0.01 |
| std::atomic, libstdc++ 13 | 0.10 | 0.03 | 0.01 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 0.17 | 0.09 | 0.06 |
| LumexLib split-count (own ptr), C++11 | 0.48 | 0.30 | 0.17 |
| LumexLib lock-based (own ptr), C++11 | 0.37 | 0.17 | 0.13 |
| LumexLib split-count r8 (own ptr), C++20 | 0.49 | 0.40 | 0.16 |
| LumexLib split-count r4 (own ptr), C++20 | 0.50 | 0.45 | 0.15 |
| LumexLib lock-based (own ptr), C++20 | 0.28 | 0.12 | 0.07 |

