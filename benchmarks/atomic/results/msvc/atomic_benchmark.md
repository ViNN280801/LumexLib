# Atomic shared pointer benchmark results

- Run 1: Intel64 Family 6 Model 154 Stepping 3, GenuineIntel, 20 logical CPUs, governor `unknown`, kernel `10` (Windows-10-10.0.26100-SP0); MSVC 1951 with MSVC STL, release build; 100 repetitions, 30 s pauses, 100 ms windows; measured 2026-10-03 in 93.8 minutes, load average unknown at the start and unknown at the end.

| Series | Selected implementation | Language mode | is_lock_free |
| --- | --- | ---: | ---: |
| LumexLib lock-based, C++11 | `lock_based_table_wait` | 201402 | no |
| LumexLib lock-based, C++20 | `lock_based_std_wait` | 202002 | no |
| LumexLib default, C++20 (wraps std) | `std_backed_std_wait` | 202002 | no |
| std::atomic, MSVC STL (MSVC 1951) | `MSVC STL (MSVC 1951)` | 202002 | no |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 11.48 | 46.20 | 122.10 | 204.27 | 291.94 | 351.74 | 394.03 | 443.88 | 493.49 | 540.20 | 583.88 |
| LumexLib lock-based, C++20 | 11.50 | 46.73 | 123.07 | 205.09 | 287.12 | 348.33 | 400.22 | 441.83 | 485.00 | 539.22 | 583.63 |
| LumexLib default, C++20 (wraps std) | 11.44 | 45.91 | 121.84 | 208.36 | 286.16 | 360.15 | 393.13 | 432.61 | 489.06 | 534.43 | 586.32 |
| std::atomic, MSVC STL (MSVC 1951) | 11.46 | 45.81 | 124.72 | 204.77 | 278.76 | 355.50 | 400.58 | 439.20 | 489.58 | 539.65 | 586.86 |
| LumexLib lock-based, C++11 / mean | 1.00 | 1.00 | 0.99 | 0.99 | 1.02 | 0.99 | 0.99 | 1.01 | 1.01 | 1.00 | 1.00 |
| LumexLib lock-based, C++20 / mean | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 | 0.98 | 1.01 | 1.01 | 0.99 | 1.00 | 1.00 |
| LumexLib default, C++20 (wraps std) / mean | 1.00 | 0.99 | 0.99 | 1.01 | 1.00 | 1.02 | 0.99 | 0.98 | 1.00 | 0.99 | 1.00 |
| std::atomic, MSVC STL (MSVC 1951) / mean | 1.00 | 0.99 | 1.01 | 1.00 | 0.97 | 1.00 | 1.01 | 1.00 | 1.00 | 1.00 | 1.00 |

## `load()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.4 (2.4-3.3) | 4.0 (3.8-5.1) | 6.4 (5.8-7.1) | 7.2 (6.4-7.8) | 7.2 (6.6-7.9) | 7.1 (6.4-7.8) | 7.4 (5.8-8.0) | 7.3 (6.1-8.1) | 7.2 (6.1-8.1) | 7.1 (6.2-7.8) | 7.0 (6.3-7.7) |
| LumexLib lock-based, C++20 | 2.4 (2.3-3.1) | 3.7 (3.5-4.9) | 6.9 (5.9-7.4) | 8.5 (7.8-9.4) | 9.5 (8.3-10.7) | 10.0 (7.5-11.0) | 10.3 (6.7-11.6) | 10.7 (6.6-12.1) | 11.6 (6.8-12.7) | 11.4 (6.5-12.8) | 11.8 (6.7-12.8) |
| LumexLib default, C++20 (wraps std) | 2.3 (2.2-3.2) | 5.5 (4.9-5.9) | 6.8 (6.3-7.6) | 7.0 (6.4-7.6) | 6.6 (6.2-7.1) | 6.3 (5.8-7.0) | 6.8 (6.3-7.3) | 6.9 (6.4-7.3) | 7.0 (6.5-7.5) | 7.1 (6.6-7.5) | 7.0 (6.7-7.6) |
| std::atomic, MSVC STL (MSVC 1951) | 2.3 (2.2-3.0) | 5.7 (5.3-6.5) | 6.9 (6.3-7.7) | 7.2 (6.5-7.9) | 6.7 (6.3-7.5) | 6.4 (6.0-6.9) | 6.7 (6.2-7.2) | 6.7 (6.3-7.2) | 7.0 (6.6-7.6) | 7.0 (6.6-7.4) | 6.9 (6.5-7.5) |

## `store()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.7 (2.6-3.3) | 3.4 (3.2-4.9) | 5.4 (4.8-5.8) | 6.6 (6.0-7.4) | 7.2 (6.5-8.0) | 7.4 (6.3-8.4) | 8.0 (5.9-8.8) | 8.2 (5.9-8.9) | 8.3 (5.9-9.0) | 8.2 (6.1-9.0) | 8.4 (6.3-9.0) |
| LumexLib lock-based, C++20 | 2.6 (2.5-3.4) | 3.4 (3.1-4.5) | 5.5 (4.8-6.2) | 6.8 (5.8-7.3) | 7.8 (7.1-8.8) | 8.8 (6.7-9.8) | 10.1 (5.9-10.9) | 10.8 (6.2-11.7) | 11.2 (6.2-12.5) | 11.6 (5.9-12.6) | 12.0 (5.9-12.8) |
| LumexLib default, C++20 (wraps std) | 2.5 (2.4-3.4) | 4.6 (4.2-5.1) | 5.6 (5.1-6.3) | 6.9 (6.3-7.6) | 7.3 (6.8-8.0) | 7.1 (6.6-7.8) | 7.4 (7.0-8.0) | 7.6 (7.2-8.1) | 7.6 (7.1-8.2) | 7.7 (7.2-8.2) | 7.9 (7.3-8.3) |
| std::atomic, MSVC STL (MSVC 1951) | 2.5 (2.4-3.1) | 4.8 (4.4-5.4) | 5.8 (5.2-6.5) | 6.9 (6.4-7.7) | 7.4 (6.9-8.1) | 7.3 (6.6-7.7) | 7.5 (6.9-8.1) | 7.3 (6.9-7.9) | 7.6 (7.1-8.2) | 7.7 (7.0-8.2) | 7.7 (7.2-8.2) |

## `exchange()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 2.7 (2.6-3.8) | 3.6 (3.1-4.9) | 5.4 (4.8-6.1) | 6.9 (6.1-7.8) | 7.4 (6.6-8.1) | 7.3 (6.5-8.3) | 8.0 (5.9-8.7) | 8.2 (5.9-9.0) | 8.3 (5.9-8.9) | 8.2 (6.0-8.9) | 8.3 (6.0-8.9) |
| LumexLib lock-based, C++20 | 2.6 (2.5-3.3) | 3.4 (3.1-4.7) | 5.7 (4.9-6.2) | 7.1 (6.2-7.9) | 8.2 (7.2-9.3) | 8.8 (6.8-9.9) | 10.1 (5.9-11.2) | 10.7 (6.1-11.8) | 11.5 (6.0-12.5) | 11.5 (5.9-12.5) | 11.9 (6.2-12.9) |
| LumexLib default, C++20 (wraps std) | 2.6 (2.5-3.5) | 4.6 (4.2-5.0) | 5.5 (5.1-6.3) | 6.8 (6.3-7.5) | 7.3 (6.9-8.0) | 7.1 (6.6-7.8) | 7.4 (6.9-7.9) | 7.5 (7.1-8.0) | 7.6 (7.0-8.1) | 7.6 (6.9-8.1) | 7.8 (7.3-8.3) |
| std::atomic, MSVC STL (MSVC 1951) | 2.6 (2.4-3.2) | 4.7 (4.3-5.2) | 5.5 (5.0-6.5) | 7.0 (6.5-7.8) | 7.5 (6.9-8.1) | 7.3 (6.7-7.8) | 7.4 (6.9-8.0) | 7.4 (6.9-8.0) | 7.6 (7.1-8.1) | 7.6 (7.0-8.2) | 7.6 (7.3-8.1) |

## `compare_exchange_strong()`, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 6.6 (6.1-9.2) | 10.5 (9.6-14.5) | 14.6 (13.4-15.6) | 16.1 (14.5-17.9) | 16.5 (14.9-18.3) | 16.5 (15.1-18.3) | 17.7 (13.8-18.8) | 17.8 (13.7-19.2) | 17.7 (13.6-19.1) | 17.7 (14.0-19.1) | 17.4 (13.9-18.6) |
| LumexLib lock-based, C++20 | 6.5 (6.2-8.8) | 10.2 (9.6-12.0) | 15.1 (13.5-16.9) | 18.1 (15.9-19.6) | 20.4 (18.1-22.6) | 21.8 (16.5-23.7) | 23.1 (14.1-25.1) | 24.0 (14.0-26.0) | 25.5 (14.4-27.1) | 25.5 (14.4-27.3) | 25.9 (14.6-27.8) |
| LumexLib default, C++20 (wraps std) | 6.5 (6.0-9.0) | 11.5 (10.2-13.9) | 15.2 (13.9-16.8) | 16.9 (15.3-18.4) | 16.4 (15.5-18.1) | 15.7 (14.6-17.5) | 16.6 (15.7-17.8) | 17.1 (16.0-18.1) | 16.9 (16.1-18.4) | 17.2 (16.2-18.6) | 17.7 (16.7-18.7) |
| std::atomic, MSVC STL (MSVC 1951) | 6.3 (6.0-9.0) | 11.3 (9.9-14.9) | 14.7 (13.6-16.7) | 17.0 (15.5-18.7) | 16.8 (15.4-18.4) | 15.9 (14.7-17.3) | 16.4 (15.2-17.6) | 16.4 (15.6-17.5) | 16.7 (15.8-18.1) | 17.1 (16.2-18.0) | 17.1 (16.1-18.6) |

Share of successful compare-exchanges:

| Series | 1 | 2 | 4 | 6 | 8 | 10 | 12 | 14 | 16 | 18 | 20 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 1.00 | 0.78 | 0.64 | 0.65 | 0.63 | 0.63 | 0.63 | 0.63 | 0.63 | 0.64 | 0.65 |
| LumexLib lock-based, C++20 | 1.00 | 0.78 | 0.62 | 0.59 | 0.59 | 0.59 | 0.57 | 0.58 | 0.58 | 0.60 | 0.58 |
| LumexLib default, C++20 (wraps std) | 1.00 | 0.85 | 0.74 | 0.75 | 0.77 | 0.78 | 0.78 | 0.77 | 0.77 | 0.76 | 0.76 |
| std::atomic, MSVC STL (MSVC 1951) | 1.00 | 0.86 | 0.75 | 0.75 | 0.77 | 0.78 | 0.78 | 0.78 | 0.77 | 0.76 | 0.76 |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS |
| --- | --- | ---: | ---: | ---: |
| `uint64_cas` | LumexLib lock-based, C++11 | 11.1 | 10.8-11.3 | 1.00 |
| `uint64_cas` | LumexLib lock-based, C++20 | 11.1 | 10.8-11.3 | 1.00 |
| `uint64_cas` | LumexLib default, C++20 (wraps std) | 11.1 | 10.8-11.4 | 1.00 |
| `uint64_cas` | std::atomic, MSVC STL (MSVC 1951) | 11.0 | 10.9-11.3 | 1.00 |
| `load` | LumexLib lock-based, C++11 | 26.4 | 25.9-27.0 | 2.39 |
| `load` | LumexLib lock-based, C++20 | 26.1 | 25.6-26.6 | 2.35 |
| `load` | LumexLib default, C++20 (wraps std) | 25.2 | 24.6-25.9 | 2.28 |
| `load` | std::atomic, MSVC STL (MSVC 1951) | 25.1 | 24.8-25.5 | 2.27 |
| `store` | LumexLib lock-based, C++11 | 29.4 | 28.8-30.3 | 2.66 |
| `store` | LumexLib lock-based, C++20 | 28.5 | 28.0-29.4 | 2.58 |
| `store` | LumexLib default, C++20 (wraps std) | 27.3 | 26.7-27.9 | 2.46 |
| `store` | std::atomic, MSVC STL (MSVC 1951) | 27.3 | 26.9-27.7 | 2.46 |
| `exchange` | LumexLib lock-based, C++11 | 29.3 | 28.8-29.9 | 2.64 |
| `exchange` | LumexLib lock-based, C++20 | 28.5 | 28.1-29.2 | 2.58 |
| `exchange` | LumexLib default, C++20 (wraps std) | 27.5 | 27.0-28.1 | 2.48 |
| `exchange` | std::atomic, MSVC STL (MSVC 1951) | 27.6 | 27.2-28.7 | 2.50 |
| `compare_exchange_strong` | LumexLib lock-based, C++11 | 69.8 | 68.5-71.8 | 6.28 |
| `compare_exchange_strong` | LumexLib lock-based, C++20 | 70.7 | 69.1-72.6 | 6.38 |
| `compare_exchange_strong` | LumexLib default, C++20 (wraps std) | 68.1 | 66.7-69.0 | 6.10 |
| `compare_exchange_strong` | std::atomic, MSVC STL (MSVC 1951) | 66.8 | 66.1-69.2 | 6.06 |
