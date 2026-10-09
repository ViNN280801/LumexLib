# Hazard pointer benchmark results

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic` (Linux-6.1.170-1-generic-x86_64-with-AstraLinux-1.7_x86-64-1.7_x86-64); 13.2.0 with LumexLib hazard_pointer, Release (NDEBUG) build; 5 repetitions, 0 s pauses, 100 ms windows; measured 2026-10-09 in 0.9 minutes, load average 9.80 9.01 7.98 at the start and 12.10 9.69 8.27 at the end.

| Series | What it is | Language mode | Progress |
| --- | --- | ---: | --- |
| LumexLib hazard_pointer | own engine, seq_cst fences | C++11 | lock-free reader |
| plain acquire load (unsafe) | lower bound of a read | C++11 | no protection |
| std::shared_mutex read lock | lock_shared / unlock_shared | C++17 | lock |
| std::atomic_load (shared_ptr) | copy and release a shared_ptr | C++11 | lock-free or locked |
| new + delete of a small object | the allocator baseline of retire | C++11 | allocator |

## Baseline

`std::atomic<uint64_t>::compare_exchange_strong` with all threads on one word, timed right before each series' block (ns per operation and thread, median). It is the same code every time, so the rows should agree; the ratio of each row to the mean of the rows shows how far the machine drifted between the blocks.

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 32.04 | 76.74 | 164.94 | 325.85 |
| plain acquire load (unsafe) | 32.79 | 74.80 | 167.90 | 326.07 |
| std::shared_mutex read lock | 32.79 | 74.80 | 167.90 | 326.07 |
| std::atomic_load (shared_ptr) | 30.65 | 68.48 | 156.63 | 296.99 |
| new + delete of a small object | 30.57 | 73.38 | 155.15 | 312.30 |
| LumexLib hazard_pointer / mean | 1.01 | 1.04 | 1.02 | 1.03 |
| plain acquire load (unsafe) / mean | 1.03 | 1.02 | 1.03 | 1.03 |
| std::shared_mutex read lock / mean | 1.03 | 1.02 | 1.03 | 1.03 |
| std::atomic_load (shared_ptr) / mean | 0.96 | 0.93 | 0.96 | 0.94 |
| new + delete of a small object / mean | 0.96 | 1.00 | 0.95 | 0.98 |

## make_hazard_pointer () and destroy, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 0.14 (0.14-0.14) | 0.07 (0.06-0.07) | 0.03 (0.03-0.04) | 0.03 (0.03-0.03) |

## protect () with a pre-made holder, then reset, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 0.20 (0.18-0.21) | 0.09 (0.09-0.09) | 0.05 (0.04-0.05) | 0.03 (0.03-0.03) |

## protect () again with the same holder (no reset), contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 0.22 (0.18-0.23) | 0.09 (0.09-0.10) | 0.05 (0.04-0.05) | 0.03 (0.03-0.03) |

## make, protect and destroy, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 0.28 (0.27-0.29) | 0.13 (0.12-0.13) | 0.07 (0.06-0.07) | 0.05 (0.05-0.05) |

## new + retire () including the inline reclamation passes, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 6.87 (6.50-7.49) | 5.57 (5.55-5.74) | 4.95 (4.82-5.01) | 4.68 (4.63-4.96) |

## read of a read-mostly pointer, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 0.20 (0.20-0.21) | 0.09 (0.08-0.09) | 0.04 (0.04-0.04) | 0.03 (0.03-0.03) |
| plain acquire load (unsafe) | 0.05 (0.05-0.05) | 0.02 (0.02-0.02) | 0.01 (0.01-0.01) | 0.00 (0.00-0.00) |
| std::shared_mutex read lock | 3.44 (2.88-3.46) | 3.19 (2.96-3.29) | 3.45 (3.30-3.55) | 4.12 (3.27-4.36) |
| std::atomic_load (shared_ptr) | 5.38 (4.55-5.62) | 3.81 (3.60-3.81) | 3.95 (3.94-4.20) | 5.22 (4.83-5.31) |

## read while one thread replaces the pointer at full speed, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 0.47 (0.45-0.66) | 0.29 (0.28-0.29) | 0.16 (0.16-0.17) | 0.08 (0.08-0.09) |
| plain acquire load (unsafe) | 0.05 (0.05-0.05) | 0.02 (0.02-0.02) | 0.01 (0.01-0.01) | 0.00 (0.00-0.01) |
| std::shared_mutex read lock | 42.73 (38.04-45.66) | 3.47 (3.31-3.63) | 2.84 (2.69-2.99) | 3.72 (2.95-3.90) |
| std::atomic_load (shared_ptr) | 5.47 (3.48-6.21) | 3.99 (3.89-4.03) | 4.43 (4.29-4.46) | 5.60 (5.12-5.63) |

## visit one node of a 64-node list, hand-over-hand, contended

Ratio to the baseline of the same block (median, with the 25th and 75th percentiles of the repetitions; lower is better).

| Series | 2 | 4 | 8 | 16 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib hazard_pointer | 0.18 (0.17-0.21) | 0.09 (0.08-0.09) | 0.04 (0.04-0.04) | 0.03 (0.03-0.03) |
| plain acquire load (unsafe) | 0.05 (0.05-0.05) | 0.02 (0.02-0.03) | 0.01 (0.01-0.01) | 0.01 (0.01-0.01) |

## Uncontended

One thread on a private object: median ns per operation, the 25th-75th percentile, and the ratio to the uncontended baseline of the same block.

| Operation | Series | Median, ns | p25-p75, ns | x uint64 CAS |
| --- | --- | ---: | ---: | ---: |
| `uint64_cas` | LumexLib hazard_pointer | 7.4 | 7.4-7.6 | 1.00 |
| `uint64_cas` | plain acquire load (unsafe) | 7.5 | 7.4-7.6 | 1.00 |
| `uint64_cas` | std::shared_mutex read lock | 7.5 | 7.4-7.6 | 1.00 |
| `uint64_cas` | std::atomic_load (shared_ptr) | 7.4 | 7.3-7.5 | 1.00 |
| `uint64_cas` | new + delete of a small object | 7.4 | 7.4-7.4 | 1.00 |
| `make_destroy` | LumexLib hazard_pointer | 3.9 | 3.9-4.1 | 0.54 |
| `protect_reset` | LumexLib hazard_pointer | 5.9 | 5.8-6.0 | 0.81 |
| `protect_kept` | LumexLib hazard_pointer | 6.0 | 5.9-6.0 | 0.81 |
| `make_protect_destroy` | LumexLib hazard_pointer | 8.1 | 8.1-8.2 | 1.09 |
| `retire` | LumexLib hazard_pointer | 45.2 | 44.9-45.8 | 6.09 |
| `read` | LumexLib hazard_pointer | 6.1 | 5.9-6.2 | 0.79 |
| `read` | plain acquire load (unsafe) | 1.6 | 1.6-1.6 | 0.21 |
| `read` | std::shared_mutex read lock | 14.8 | 14.8-14.8 | 1.99 |
| `read` | std::atomic_load (shared_ptr) | 24.7 | 24.6-25.0 | 3.36 |
| `list_walk` | LumexLib hazard_pointer | 6.0 | 5.8-6.1 | 0.79 |
| `list_walk` | plain acquire load (unsafe) | 1.5 | 1.5-1.5 | 0.20 |
| `new_delete` | new + delete of a small object | 8.9 | 8.9-9.0 | 1.21 |
| `retire_p50` | LumexLib hazard_pointer | 30.0 | 30.0-30.0 | 3.82 |
| `retire_p99` | LumexLib hazard_pointer | 51.0 | 51.0-52.0 | 6.50 |
| `retire_p999` | LumexLib hazard_pointer | 10957.0 | 6811.0-12865.0 | 1396.00 |
| `retire_max` | LumexLib hazard_pointer | 31198.0 | 26722.0-119613.0 | 3818.85 |
