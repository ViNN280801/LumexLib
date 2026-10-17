# Time of a single store under a read load

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic`; GCC 13.2 with libstdc++ 13, release build; 10 repetitions of 100000 timed stores per series and thread count, 5 s pauses; clock `std::chrono::steady_clock`; threads pinned, one CPU each; measured 2026-10-15 in 5.2 minutes, load average 3.72 11.55 11.49 at the start and 35.75 26.60 18.06 at the end.

One thread (the writer) stores alternating values in a loop and times every `store ()` call on its own; the other threads (1 less than the thread count) `load ()` in a loop. Times are nanoseconds of one call, not rates. Every cell is the median over the repetitions of the statistic of one repetition, except the maximum, which also gives the longest store of all repetitions together, and the count of stores over 1 ms, which is summed over all repetitions (with its share of all stores timed). A store over 1 ms is far above any cost of the engine at these thread counts and is also what a preempted writer looks like: the count does not tell the two apart. Up to 20 threads are the 20 logical CPUs; 40 and 60 are oversubscribed.

## Median of the time of one store, ns

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 390 | 1246 | 1370 | 2030 |
| LumexLib lock-free, C++20 | 260 | 388 | 480 | 466 |
| std::atomic, libstdc++ 13 | 81 | 3104 | 7702 | 8069 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 63 | 87 | 174 | 201 |
| LumexLib split-count r8 (own ptr), C++20 | 332 | 856 | 1339 | 1284 |
| LumexLib lock-based (own ptr), C++20 | 391 | 1198 | 1300 | 1838 |

## 90th percentile, ns

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 1994 | 8302 | 14127 | 58084 |
| LumexLib lock-free, C++20 | 397 | 726 | 987 | 932 |
| std::atomic, libstdc++ 13 | 869 | 9268 | 23519 | 23998 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 260 | 53046 | 53632 | 106779 |
| LumexLib split-count r8 (own ptr), C++20 | 567 | 1766 | 2778 | 2582 |
| LumexLib lock-based (own ptr), C++20 | 1990 | 8068 | 13934 | 59176 |

## 99th percentile, ns

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 4126 | 13494 | 26178 | 104858 |
| LumexLib lock-free, C++20 | 602 | 1262 | 1838 | 1828 |
| std::atomic, libstdc++ 13 | 2379 | 18278 | 51800 | 50722 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 106068 | 140158 | 160286 | 214570 |
| LumexLib split-count r8 (own ptr), C++20 | 800 | 3223 | 6338 | 4146 |
| LumexLib lock-based (own ptr), C++20 | 4134 | 13518 | 25882 | 109670 |

## 99.9th percentile, ns

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 6424 | 26026 | 58152 | 171924 |
| LumexLib lock-free, C++20 | 780 | 2404 | 26245 | 289696 |
| std::atomic, libstdc++ 13 | 5840 | 34534 | 129610 | 31219722 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 162282 | 264738 | 273802 | 325443 |
| LumexLib split-count r8 (own ptr), C++20 | 1026 | 4924 | 17824 | 5666 |
| LumexLib lock-based (own ptr), C++20 | 6424 | 26216 | 70571 | 183550 |

## 99.99th percentile, ns

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 8398 | 61150 | 627279 | 681679 |
| LumexLib lock-free, C++20 | 81505 | 340293 | 610972 | 8557892 |
| std::atomic, libstdc++ 13 | 10098 | 135697 | 747992 | 81324818 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 294531 | 502024 | 467915 | 585569 |
| LumexLib split-count r8 (own ptr), C++20 | 1729 | 6618 | 39420 | 16145126 |
| LumexLib lock-based (own ptr), C++20 | 8300 | 90454 | 627074 | 726634 |

## The longest store, ns

Median over the repetitions of the longest store of a repetition, then the longest store of all repetitions together in brackets.

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 26711 (47279) | 134962 (174016) | 687074 (1580222) | 1673827 (11556799) |
| LumexLib lock-free, C++20 | 106387 (155453) | 374194 (486091) | 1680378 (7888919) | 27609754 (32311816) |
| std::atomic, libstdc++ 13 | 21604 (62033) | 164090 (174386) | 1386938 (8115619) | 141188384 (164998079) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 1048480 (3533241) | 1462368 (6586284) | 1079178 (4024167) | 1557318 (7342423) |
| LumexLib split-count r8 (own ptr), C++20 | 21387 (67489) | 16540 (29388) | 170104 (2118476) | 26806870 (36008742) |
| LumexLib lock-based (own ptr), C++20 | 20342 (57089) | 161248 (1348599) | 1289068 (13647373) | 1756620 (11516715) |

## Stores over 1 ms

Stores that took more than a millisecond, summed over all the repetitions, with the share of all stores timed.

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 0 (0.0000 %) | 0 (0.0000 %) | 5 (0.0005 %) | 23 (0.0023 %) |
| LumexLib lock-free, C++20 | 0 (0.0000 %) | 0 (0.0000 %) | 108 (0.0108 %) | 204 (0.0204 %) |
| std::atomic, libstdc++ 13 | 0 (0.0000 %) | 0 (0.0000 %) | 35 (0.0035 %) | 3606 (0.3606 %) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 6 (0.0006 %) | 13 (0.0013 %) | 14 (0.0014 %) | 39 (0.0039 %) |
| LumexLib split-count r8 (own ptr), C++20 | 0 (0.0000 %) | 0 (0.0000 %) | 9 (0.0009 %) | 208 (0.0208 %) |
| LumexLib lock-based (own ptr), C++20 | 0 (0.0000 %) | 1 (0.0001 %) | 13 (0.0013 %) | 26 (0.0026 %) |

## Preemption of the writer

Involuntary context switches of the writer thread during its timed stores (the scheduler took it off its CPU), summed over all the repetitions, and in brackets how many of the stores over 1 ms were followed by an advance of that counter since the previous reading (the counter is read only after a store over 1 ms, so a switch that took less than 1 ms can be credited to the next slow store: the bracket is an upper bound of the slow stores that a preemption explains). Where it is 0 on a platform, the system does not report it.

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 22 (0 of 0) | 160 (0 of 0) | 12361 (5 of 5) | 1000291 (23 of 23) |
| LumexLib lock-free, C++20 | 7 (0 of 0) | 17 (0 of 0) | 4348 (108 of 108) | 2622 (204 of 204) |
| std::atomic, libstdc++ 13 | 13 (0 of 0) | 72 (0 of 0) | 47449 (35 of 35) | 19433 (3430 of 3606) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 7 (2 of 6) | 8 (2 of 13) | 22 (4 of 14) | 61 (8 of 39) |
| LumexLib split-count r8 (own ptr), C++20 | 13 (0 of 0) | 16 (0 of 0) | 8245 (9 of 9) | 863 (208 of 208) |
| LumexLib lock-based (own ptr), C++20 | 15 (0 of 0) | 158 (1 of 1) | 9644 (13 of 13) | 965797 (26 of 26) |

## Cost of the timer, ns

Median of an empty pair of `steady_clock::now ()` reads on the writer thread, with the readers running, right before its stores; the median over the repetitions. It is part of every time above.

| Series | 4 | 12 | 20 | 60 |
| --- | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 13 | 12 | 13 | 14 |
| LumexLib lock-free, C++20 | 13 | 12 | 12 | 12 |
| std::atomic, libstdc++ 13 | 13 | 12 | 12 | 11 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 13 | 13 | 13 | 13 |
| LumexLib split-count r8 (own ptr), C++20 | 14 | 32 | 28 | 15 |
| LumexLib lock-based (own ptr), C++20 | 13 | 12 | 13 | 13 |
