# Time of a single store under a read load

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`, kernel `6.1.170-1-generic`; GCC 13.2 with libstdc++ 13, release build; 40 repetitions of 100000 timed stores per series and thread count, 10 s pauses; clock `std::chrono::steady_clock`; threads not pinned; measured 2026-10-15 in 43.2 minutes, load average 2.20 2.75 3.24 at the start and 26.99 25.07 22.83 at the end.

One thread (the writer) stores alternating values in a loop and times every `store ()` call on its own; the other threads (1 less than the thread count) `load ()` in a loop. Times are nanoseconds of one call, not rates. Every cell is the median over the repetitions of the statistic of one repetition, except the maximum, which also gives the longest store of all repetitions together, and the count of stores over 1 ms, which is summed over all repetitions (with its share of all stores timed). A store over 1 ms is far above any cost of the engine at these thread counts and is also what a preempted writer looks like: the count does not tell the two apart. Up to 20 threads are the 20 logical CPUs; 40 and 60 are oversubscribed.

## Median of the time of one store, ns

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 216 | 521 | 940 | 1838 | 1702 | 2420 | 2530 |
| LumexLib lock-free, C++20 | 245 | 310 | 392 | 474 | 532 | 610 | 536 |
| std::atomic, libstdc++ 13 | 60 | 233 | 2954 | 27834 | 7314 | 7659 | 7848 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 144 | 560 | 758 | 698 | 488 | 388 | 316 |
| LumexLib split-count r8 (own ptr), C++20 | 134 | 320 | 639 | 4532 | 1614 | 1674 | 1626 |
| LumexLib lock-based (own ptr), C++20 | 270 | 647 | 988 | 1599 | 2020 | 2536 | 2756 |

## 90th percentile, ns

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 412 | 2957 | 5204 | 10186 | 13397 | 37676 | 61008 |
| LumexLib lock-free, C++20 | 288 | 440 | 739 | 888 | 1110 | 1288 | 1126 |
| std::atomic, libstdc++ 13 | 64 | 1769 | 11188 | 71102 | 22182 | 23142 | 23522 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 52652 | 61080 | 64001 | 64513 | 67338 | 89364 | 83254 |
| LumexLib split-count r8 (own ptr), C++20 | 176 | 512 | 1562 | 12286 | 3694 | 3731 | 3645 |
| LumexLib lock-based (own ptr), C++20 | 444 | 3122 | 5394 | 9589 | 13909 | 40380 | 64288 |

## 99th percentile, ns

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 1228 | 3758 | 8456 | 18630 | 33317 | 76892 | 122654 |
| LumexLib lock-free, C++20 | 322 | 610 | 1262 | 1732 | 2131 | 2324 | 2018 |
| std::atomic, libstdc++ 13 | 204 | 4450 | 24107 | 131434 | 41978 | 45302 | 45537 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 107906 | 181916 | 192396 | 216569 | 227180 | 228718 | 216533 |
| LumexLib split-count r8 (own ptr), C++20 | 230 | 978 | 4188 | 23962 | 6935 | 6836 | 6672 |
| LumexLib lock-based (own ptr), C++20 | 867 | 3834 | 8744 | 18240 | 34580 | 82584 | 124840 |

## 99.9th percentile, ns

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 1955 | 4524 | 12244 | 30662 | 57262 | 126700 | 201008 |
| LumexLib lock-free, C++20 | 395 | 830 | 2784 | 10932 | 7936 | 220306 | 323869 |
| std::atomic, libstdc++ 13 | 542 | 20832 | 48326 | 206170 | 68574 | 13935238 | 30379128 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 218829 | 331407 | 378632 | 380812 | 404867 | 384004 | 346356 |
| LumexLib split-count r8 (own ptr), C++20 | 288 | 1706 | 6826 | 34648 | 10962 | 10319 | 10397 |
| LumexLib lock-based (own ptr), C++20 | 1887 | 4666 | 12900 | 34466 | 63056 | 132666 | 202520 |

## 99.99th percentile, ns

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 2842 | 5696 | 18200 | 53883 | 97988 | 297022 | 487150 |
| LumexLib lock-free, C++20 | 2616 | 95498 | 229412 | 891882 | 639685 | 747326 | 11657273 |
| std::atomic, libstdc++ 13 | 2392 | 173647 | 796870 | 380146 | 514868 | 54296912 | 128793107 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 377086 | 538440 | 571739 | 624078 | 649150 | 570968 | 522981 |
| LumexLib split-count r8 (own ptr), C++20 | 2450 | 3368 | 11082 | 49052 | 27109 | 11900794 | 24003700 |
| LumexLib lock-based (own ptr), C++20 | 2830 | 6406 | 20436 | 52857 | 105094 | 264988 | 837469 |

## The longest store, ns

Median over the repetitions of the longest store of a repetition, then the longest store of all repetitions together in brackets.

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 16956 (173540) | 47132 (751580) | 127374 (17669645) | 207116 (8765357) | 951666 (31662505) | 2456132 (18078578) | 3796136 (51808392) |
| LumexLib lock-free, C++20 | 14090 (687864) | 129492 (259442) | 329237 (13417347) | 1040870 (27777326) | 792652 (24723009) | 17159370 (32007174) | 29010336 (44674377) |
| std::atomic, libstdc++ 13 | 9305 (249783) | 982250 (6570558) | 3503909 (33604777) | 2075136 (73130001) | 9884874 (153505033) | 98303384 (180040909) | 248183986 (392031039) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 589756 (9334069) | 893530 (9334365) | 1377386 (15346957) | 1761435 (19496402) | 1439163 (10028987) | 1300590 (13602905) | 984592 (8409308) |
| LumexLib split-count r8 (own ptr), C++20 | 6750 (1250985) | 12404 (572636) | 22254 (54325897) | 62765 (14563780) | 676860 (28018029) | 20599149 (32013013) | 34611836 (48015468) |
| LumexLib lock-based (own ptr), C++20 | 20284 (2190149) | 39130 (1324532) | 161990 (23867434) | 185674 (23627429) | 623944 (14945202) | 2115504 (19273185) | 5274746 (29521444) |

## Stores over 1 ms

Stores that took more than a millisecond, summed over all the repetitions, with the share of all stores timed.

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 0 (0.0000 %) | 0 (0.0000 %) | 23 (0.0006 %) | 4 (0.0001 %) | 72 (0.0018 %) | 157 (0.0039 %) | 317 (0.0079 %) |
| LumexLib lock-free, C++20 | 0 (0.0000 %) | 0 (0.0000 %) | 5 (0.0001 %) | 1353 (0.0338 %) | 265 (0.0066 %) | 538 (0.0135 %) | 906 (0.0226 %) |
| std::atomic, libstdc++ 13 | 0 (0.0000 %) | 620 (0.0155 %) | 32421 (0.8105 %) | 27252 (0.6813 %) | 420 (0.0105 %) | 6226 (0.1557 %) | 9135 (0.2284 %) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 10 (0.0003 %) | 30 (0.0008 %) | 100 (0.0025 %) | 123 (0.0031 %) | 126 (0.0032 %) | 43 (0.0011 %) | 37 (0.0009 %) |
| LumexLib split-count r8 (own ptr), C++20 | 1 (0.0000 %) | 0 (0.0000 %) | 4 (0.0001 %) | 24 (0.0006 %) | 99 (0.0025 %) | 730 (0.0182 %) | 1362 (0.0340 %) |
| LumexLib lock-based (own ptr), C++20 | 1 (0.0000 %) | 2 (0.0001 %) | 29 (0.0007 %) | 48 (0.0012 %) | 78 (0.0019 %) | 138 (0.0034 %) | 422 (0.0106 %) |

## Preemption of the writer

Involuntary context switches of the writer thread during its timed stores (the scheduler took it off its CPU), summed over all the repetitions, and in brackets how many of the stores over 1 ms were followed by an advance of that counter since the previous reading (the counter is read only after a store over 1 ms, so a switch that took less than 1 ms can be credited to the next slow store: the bracket is an upper bound of the slow stores that a preemption explains). Where it is 0 on a platform, the system does not report it.

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 45 (0 of 0) | 1024 (0 of 0) | 3426 (23 of 23) | 17808 (4 of 4) | 368723 (71 of 72) | 2883397 (157 of 157) | 3941428 (317 of 317) |
| LumexLib lock-free, C++20 | 117 (0 of 0) | 199 (0 of 0) | 148 (4 of 5) | 434 (153 of 1353) | 667 (61 of 265) | 1233 (478 of 538) | 1242 (719 of 906) |
| std::atomic, libstdc++ 13 | 38 (0 of 0) | 370 (66 of 620) | 2400 (336 of 32421) | 7073 (768 of 27252) | 5815 (370 of 420) | 16524 (5743 of 6226) | 26138 (8772 of 9135) |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 285 (8 of 10) | 1048 (26 of 30) | 1529 (57 of 100) | 2681 (71 of 123) | 8075 (72 of 126) | 8119 (41 of 43) | 10140 (33 of 37) |
| LumexLib split-count r8 (own ptr), C++20 | 30 (0 of 1) | 63 (0 of 0) | 164 (4 of 4) | 1484 (24 of 24) | 1801 (99 of 99) | 1395 (730 of 730) | 1892 (1362 of 1362) |
| LumexLib lock-based (own ptr), C++20 | 78 (1 of 1) | 532 (2 of 2) | 23872 (28 of 29) | 54978 (47 of 48) | 319775 (78 of 78) | 3071922 (137 of 138) | 3848368 (422 of 422) |

## Cost of the timer, ns

Median of an empty pair of `steady_clock::now ()` reads on the writer thread, with the readers running, right before its stores; the median over the repetitions. It is part of every time above.

| Series | 2 | 4 | 8 | 12 | 20 | 40 | 60 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++20 | 11 | 12 | 12 | 18 | 12 | 13 | 13 |
| LumexLib lock-free, C++20 | 11 | 12 | 13 | 18 | 11 | 12 | 12 |
| std::atomic, libstdc++ 13 | 11 | 12 | 12 | 18 | 12 | 12 | 12 |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 11 | 12 | 13 | 13 | 13 | 13 | 13 |
| LumexLib split-count r8 (own ptr), C++20 | 12 | 21 | 62 | 35 | 38 | 15 | 15 |
| LumexLib lock-based (own ptr), C++20 | 11 | 12 | 12 | 18 | 13 | 13 | 13 |
