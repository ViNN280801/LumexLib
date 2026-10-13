# Destruction latency

- Run 1: 12th Gen Intel(R) Core(TM) i7-12700K, 20 logical CPUs, governor `powersave`; GCC 13.2 with libstdc++ 13, release build; 100 repetitions of 20000 replaced objects per series and thread count; measured 2026-10-13.

A thread replaces the stored value with a new object that has a deleter, 20000 times in a row; the latency of an object is the time from the start of the store that replaced it to the run of its deleter (in nanoseconds). With more than one thread the others load in a loop, so the last owner can be a reader. The table gives, for each series, the median over the repetitions of: the median, the 99th percentile and the maximum of the latencies of the objects destroyed by the time the loop ended; the share of objects destroyed before the replacing store returned; the peak number of replaced objects not yet destroyed (after a store returned); and the share still pending when the loop ended (not in the percentiles).

## 1 thread

| Series | median, ns | p99, ns | max, ns | destroyed in the call | peak backlog | pending at the end |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 50 | 76 | 1013 | 100.0 % | 0 | 0.0 % |
| LumexLib lock-based, C++20 | 42 | 68 | 746 | 100.0 % | 0 | 0.0 % |
| LumexLib lock-free, C++11 | 60 | 78 | 4834 | 100.0 % | 0 | 0.0 % |
| LumexLib lock-free, C++20 | 61 | 86 | 1996 | 100.0 % | 0 | 0.0 % |
| LumexLib lock-free, C++20, deferred | 116350 | 253538 | 326970 | 0.1 % | 998 | 0.0 % |
| LumexLib common name, C++20 (lock-free) | 52 | 74 | 1672 | 100.0 % | 0 | 0.0 % |
| LumexLib std-backed, C++20 (wraps std) | 40 | 62 | 890 | 100.0 % | 0 | 0.0 % |
| std::atomic, libstdc++ 13 | 46 | 64 | 864 | 100.0 % | 0 | 0.0 % |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 49 | 75 | 956 | 100.0 % | 0 | 0.0 % |
| LumexLib split-count (own ptr), C++11 | 46 | 72 | 1168 | 100.0 % | 0 | 0.0 % |
| LumexLib lock-based (own ptr), C++11 | 52 | 81 | 1448 | 100.0 % | 0 | 0.0 % |
| LumexLib split-count r8 (own ptr), C++20 | 44 | 66 | 1156 | 100.0 % | 0 | 0.0 % |
| LumexLib split-count r4 (own ptr), C++20 | 44 | 64 | 1070 | 100.0 % | 0 | 0.0 % |
| LumexLib lock-based (own ptr), C++20 | 52 | 79 | 1033 | 100.0 % | 0 | 0.0 % |

## 4 threads

| Series | median, ns | p99, ns | max, ns | destroyed in the call | peak backlog | pending at the end |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| LumexLib lock-based, C++11 | 343 | 6676 | 39481 | 83.1 % | 1 | 0.0 % |
| LumexLib lock-based, C++20 | 582 | 3736 | 10866 | 76.4 % | 1 | 0.0 % |
| LumexLib lock-free, C++11 | 276 | 817232 | 938500 | 61.8 % | 998 | 2.5 % |
| LumexLib lock-free, C++20 | 264 | 892145 | 1103050 | 61.9 % | 998 | 2.7 % |
| LumexLib lock-free, C++20, deferred | 145968 | 297546 | 372185 | 0.1 % | 998 | 0.0 % |
| LumexLib common name, C++20 (lock-free) | 271 | 947128 | 1148980 | 62.5 % | 998 | 2.7 % |
| LumexLib std-backed, C++20 (wraps std) | 600 | 3674 | 118456 | 100.0 % | 0 | 0.0 % |
| std::atomic, libstdc++ 13 | 630 | 4399 | 122605 | 100.0 % | 0 | 0.0 % |
| boost::atomic_shared_ptr (spinlock), Boost 1_92 | 143 | 62670 | 253716 | 100.0 % | 0 | 0.0 % |
| LumexLib split-count (own ptr), C++11 | 446 | 1067 | 5260 | 13.0 % | 1 | 0.0 % |
| LumexLib lock-based (own ptr), C++11 | 362 | 7407 | 43694 | 81.4 % | 1 | 0.0 % |
| LumexLib split-count r8 (own ptr), C++20 | 412 | 776 | 6360 | 10.5 % | 1 | 0.0 % |
| LumexLib split-count r4 (own ptr), C++20 | 410 | 733 | 8740 | 10.2 % | 1 | 0.0 % |
| LumexLib lock-based (own ptr), C++20 | 596 | 3644 | 12708 | 73.2 % | 1 | 0.0 % |
