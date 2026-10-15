# Atomic shared pointer benchmark results

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic` (Linux-6.1.170-1-generic-x86_64-with-AstraLinux-1.7_x86-64-1.7_x86-64); GCC 13.2 with libstdc++ 13, release build; 50 repetitions, 10 s pauses, 100 ms windows; measured 2026-10-13 in 26.8 minutes, load average 0.84 2.41 4.40 at the start and 10.36 8.91 7.38 at the end.

| Series | Selected implementation | Language mode | is_lock_free |
| --- | --- | ---: | ---: |
| LumexLib split-count weak (own ptr), C++20 | `split_count_std_wait` | 202002 | yes |
| LumexLib lock-based weak (own ptr), C++20 | `lock_based_lumex_std_wait` | 202002 | no |
| LumexLib lock-based weak (std ptr), C++20 | `lock_based_std_wait` | 202002 | no |
| LumexLib lock-free weak (box), C++20 | `lock_free_std_wait` | 202002 | yes |
| LumexLib std-backed weak, C++20 (wraps std) | `std_backed_std_wait` | 202002 | no |
| std::atomic<std::weak_ptr>, the standard library | `std` | 202002 | no |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count weak (own ptr), C++20 | 6.74 | 31.47 | 62.88 | 98.28 | 133.90 | 171.34 | 204.69 | 233.14 | 258.62 | 282.20 | 303.97 |
| LumexLib lock-based weak (own ptr), C++20 | 6.73 | 31.47 | 62.42 | 97.95 | 134.07 | 171.15 | 205.58 | 234.23 | 258.08 | 280.31 | 304.14 |
| LumexLib lock-based weak (std ptr), C++20 | 6.72 | 31.14 | 62.89 | 97.63 | 134.01 | 170.45 | 204.66 | 233.49 | 258.42 | 278.27 | 305.27 |
| LumexLib lock-free weak (box), C++20 | 6.73 | 31.54 | 63.26 | 97.40 | 133.25 | 171.30 | 206.16 | 232.03 | 257.89 | 278.55 | 298.81 |
| LumexLib std-backed weak, C++20 (wraps std) | 6.74 | 31.58 | 62.69 | 97.61 | 134.50 | 171.57 | 202.71 | 232.83 | 259.02 | 279.03 | 297.90 |
| std::atomic<std::weak_ptr>, the standard library | 6.71 | 31.44 | 63.24 | 97.40 | 134.44 | 171.33 | 206.08 | 235.17 | 260.96 | 279.67 | 300.48 |
| LumexLib split-count weak (own ptr), C++20 / mean | 1.00 | 1.00 | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.01 | 1.01 |
| LumexLib lock-based weak (own ptr), C++20 / mean | 1.00 | 1.00 | 0.99 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.01 |
| LumexLib lock-based weak (std ptr), C++20 / mean | 1.00 | 0.99 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.01 |
| LumexLib lock-free weak (box), C++20 / mean | 1.00 | 1.00 | 1.01 | 1.00 | 0.99 | 1.00 | 1.01 | 0.99 | 1.00 | 1.00 | 0.99 |
| LumexLib std-backed weak, C++20 (wraps std) / mean | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 | 0.99 | 1.00 | 1.00 | 1.00 | 0.99 |
| std::atomic<std::weak_ptr>, the standard library / mean | 1.00 | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 | 1.01 | 1.01 | 1.01 | 1.00 | 1.00 |

## `weak_load()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count weak (own ptr), C++20 | 3.7 (3.7-3.8) | 6.4 (6.1-6.8) | 6.2 (6.0-6.7) | 6.4 (6.1-6.7) | 6.6 (6.3-7.2) | 7.3 (6.8-7.6) | 7.5 (6.9-7.9) | 7.9 (7.4-8.2) | 8.2 (7.8-8.7) | 8.8 (8.5-9.4) | 9.3 (8.8-10.0) |
| LumexLib lock-based weak (own ptr), C++20 | 2.2 (2.2-2.2) | 3.6 (3.4-3.9) | 4.7 (4.5-4.9) | 6.2 (5.6-6.4) | 7.8 (7.5-8.4) | 9.8 (8.7-10.3) | 10.6 (9.4-11.9) | 12.7 (11.6-14.5) | 13.5 (12.0-15.9) | 14.5 (12.8-15.8) | 14.2 (12.3-17.0) |
| LumexLib lock-based weak (std ptr), C++20 | 2.4 (2.3-2.4) | 3.3 (3.1-3.7) | 5.0 (4.7-5.2) | 6.2 (5.8-6.6) | 7.7 (7.4-8.3) | 9.3 (8.5-9.8) | 10.3 (9.0-11.3) | 12.6 (11.1-13.5) | 13.7 (12.1-15.5) | 14.3 (13.1-16.5) | 14.2 (13.3-16.2) |
| LumexLib lock-free weak (box), C++20 | 2.4 (2.4-2.4) | 1.9 (1.8-2.0) | 1.9 (1.8-2.0) | 1.9 (1.8-1.9) | 1.9 (1.8-2.0) | 2.0 (1.8-2.1) | 2.0 (1.8-2.1) | 2.0 (1.9-2.1) | 2.1 (2.0-2.2) | 2.2 (2.1-2.3) | 2.2 (2.1-2.4) |
| LumexLib std-backed weak, C++20 (wraps std) | 2.4 (2.4-2.4) | 2.9 (2.7-3.3) | 6.4 (5.9-6.7) | 9.1 (8.4-9.7) | 12.1 (11.1-13.0) | 13.4 (12.5-14.3) | 14.2 (13.2-14.9) | 14.9 (13.9-15.9) | 16.2 (15.5-17.1) | 17.9 (17.0-19.3) | 22.9 (21.1-24.9) |
| std::atomic<std::weak_ptr>, the standard library | 2.4 (2.4-2.4) | 3.0 (2.8-3.3) | 6.2 (5.8-6.5) | 8.9 (8.3-9.6) | 12.2 (11.3-12.9) | 13.3 (12.6-14.1) | 14.2 (13.3-15.3) | 14.8 (14.3-15.8) | 16.1 (15.0-16.8) | 18.1 (17.0-19.7) | 22.8 (20.4-24.6) |

## `weak_load_lock()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib split-count weak (own ptr), C++20 | 5.0 (5.0-5.0) | 6.9 (6.6-7.5) | 7.5 (7.1-7.7) | 7.7 (7.3-8.1) | 7.9 (7.5-8.5) | 8.7 (8.0-8.9) | 8.7 (8.1-9.2) | 9.1 (8.5-9.5) | 9.6 (9.2-10.1) | 10.1 (9.8-11.0) | 10.7 (10.2-11.7) |
| LumexLib lock-based weak (own ptr), C++20 | 3.6 (3.6-3.6) | 4.7 (4.3-5.1) | 7.6 (7.3-8.2) | 8.9 (8.5-9.4) | 10.4 (9.6-11.2) | 12.4 (11.4-13.0) | 13.5 (12.7-14.3) | 15.1 (14.1-16.1) | 16.4 (15.1-17.5) | 17.3 (16.2-19.0) | 17.2 (16.0-19.2) |
| LumexLib lock-based weak (std ptr), C++20 | 4.0 (4.0-4.0) | 5.0 (4.6-5.4) | 7.9 (7.6-8.5) | 9.5 (9.1-10.0) | 10.9 (10.4-11.6) | 12.9 (11.8-13.4) | 14.1 (12.9-14.9) | 15.3 (14.3-16.0) | 16.8 (15.0-17.8) | 17.1 (16.2-18.8) | 17.9 (16.6-19.2) |
| LumexLib lock-free weak (box), C++20 | 3.9 (3.9-3.9) | 4.7 (4.3-5.0) | 4.9 (4.5-5.1) | 4.7 (4.5-5.0) | 4.6 (4.4-4.9) | 4.9 (4.6-5.1) | 4.9 (4.7-5.1) | 5.1 (4.9-5.4) | 5.3 (5.1-5.6) | 5.5 (5.3-5.8) | 5.8 (5.3-6.0) |
| LumexLib std-backed weak, C++20 (wraps std) | 4.1 (4.1-4.1) | 5.8 (5.4-6.4) | 8.4 (7.6-9.0) | 10.8 (9.8-12.0) | 14.6 (13.5-15.8) | 19.3 (17.5-21.0) | 22.3 (20.2-23.8) | 25.0 (23.5-27.0) | 28.0 (26.9-29.2) | 31.0 (29.2-31.9) | 33.7 (31.3-36.0) |
| std::atomic<std::weak_ptr>, the standard library | 4.1 (4.1-4.1) | 5.8 (5.5-6.4) | 8.2 (7.7-8.7) | 10.5 (9.9-11.6) | 14.5 (13.1-15.9) | 19.5 (17.4-20.6) | 22.2 (20.6-23.7) | 25.1 (23.5-26.1) | 27.9 (26.3-29.0) | 30.9 (29.0-32.4) | 33.7 (31.8-35.5) |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.
The last two columns are the median cycles and instructions per operation of the thread in user mode (`perf_event_open`; 0 where the counter was not available).

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS | cycles | instructions |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `uint64_cas` | LumexLib split-count weak (own ptr), C++20 | 6.7 | 6.7-6.8 | 1.00  33.1 | 13.0 |
| `uint64_cas` | LumexLib lock-based weak (own ptr), C++20 | 6.7 | 6.7-6.8 | 1.00  33.1 | 13.0 |
| `uint64_cas` | LumexLib lock-based weak (std ptr), C++20 | 6.7 | 6.7-6.8 | 1.00  33.1 | 13.0 |
| `uint64_cas` | LumexLib lock-free weak (box), C++20 | 6.7 | 6.7-6.8 | 1.00  33.1 | 13.0 |
| `uint64_cas` | LumexLib std-backed weak, C++20 (wraps std) | 6.7 | 6.7-6.8 | 1.00  33.1 | 13.0 |
| `uint64_cas` | std::atomic<std::weak_ptr>, the standard library | 6.7 | 6.7-6.8 | 1.00  33.1 | 13.0 |
| `weak_load` | LumexLib split-count weak (own ptr), C++20 | 25.2 | 25.0-27.5 | 3.74  123.9 | 115.0 |
| `weak_load` | LumexLib lock-based weak (own ptr), C++20 | 14.8 | 14.8-15.0 | 2.21  73.2 | 24.0 |
| `weak_load` | LumexLib lock-based weak (std ptr), C++20 | 15.9 | 15.8-16.0 | 2.36  78.0 | 33.0 |
| `weak_load` | LumexLib lock-free weak (box), C++20 | 16.3 | 16.2-16.4 | 2.42  80.1 | 107.0 |
| `weak_load` | LumexLib std-backed weak, C++20 (wraps std) | 16.0 | 15.9-16.1 | 2.36  78.5 | 25.0 |
| `weak_load` | std::atomic<std::weak_ptr>, the standard library | 15.9 | 15.8-16.1 | 2.37  78.5 | 25.0 |
| `weak_load_lock` | LumexLib split-count weak (own ptr), C++20 | 33.6 | 33.4-34.2 | 5.01  165.6 | 131.0 |
| `weak_load_lock` | LumexLib lock-based weak (own ptr), C++20 | 24.4 | 24.3-24.6 | 3.63  120.1 | 38.0 |
| `weak_load_lock` | LumexLib lock-based weak (std ptr), C++20 | 27.0 | 26.8-27.2 | 4.01  133.0 | 54.0 |
| `weak_load_lock` | LumexLib lock-free weak (box), C++20 | 26.1 | 25.9-26.3 | 3.88  128.5 | 128.0 |
| `weak_load_lock` | LumexLib std-backed weak, C++20 (wraps std) | 27.4 | 27.2-27.7 | 4.06  134.9 | 51.0 |
| `weak_load_lock` | std::atomic<std::weak_ptr>, the standard library | 27.4 | 27.3-27.7 | 4.07  134.9 | 51.0 |
