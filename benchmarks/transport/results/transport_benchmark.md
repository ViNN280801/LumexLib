# Transport benchmarks

- clang: machine="12th Gen Intel(R) Core(TM) i7-12700K" logical_cpus=20; compiler="Clang 23.1.0" cplusplus=201103; build=NDEBUG
- gcc: machine="12th Gen Intel(R) Core(TM) i7-12700K" logical_cpus=20; compiler="GCC 13.2.0" cplusplus=201103; build=NDEBUG

## Budgets

| Budget | Toolchain | Measured | Verdict | Load |
| --- | --- | --- | --- | --- |
| Log call below the threshold: under 2 ns | clang | 0.86 ns | met | 2.00 |
| Log call below the threshold: under 2 ns | gcc | 0.94 ns | met | 1.42 |
| Dialogue round trip on mocks (async, virtual clock): under 50 us | clang | 6.3 us | met | 1.83 |
| Dialogue round trip on mocks (async, virtual clock): under 50 us | gcc | 6.1 us | met | 1.42 |
| Dialogue round trip on mocks (sync call, runner thread): under 50 us | clang | 14.9 us | met | 1.76 |
| Dialogue round trip on mocks (sync call, runner thread): under 50 us | gcc | 15.4 us | met | 1.55 |
| describe of a 100-byte frame: under 5 us | clang | 0.88 us | met | 2.00 |
| describe of a 100-byte frame: under 5 us | gcc | 0.87 us | met | 2.12 |
| Codecs at 2420 bytes: at least 100 MB/s (the slowest shown) | clang | delimiter_2420_decode at 531 MB/s | met | 1.82 |
| Codecs at 2420 bytes: at least 100 MB/s (the slowest shown) | gcc | cobs_2420_encode at 462 MB/s | met | 2.10 |
| 16 devices on one io_runner thread, real sockets and timers, a whole frame per write, 2420 bytes at 25 Hz: under 10 % of a core | clang | 3.15 % | met | 1.68 |
| 16 devices on one io_runner thread, real sockets and timers, a whole frame per write, 2420 bytes at 25 Hz: under 10 % of a core | gcc | 2.52 % | met | 1.98 |
| 16 devices on one io_runner thread, real sockets and timers, a whole frame per write, 2420 bytes at 10 Hz: under 10 % of a core | clang | 1.35 % | met | 2.03 |
| 16 devices on one io_runner thread, real sockets and timers, a whole frame per write, 2420 bytes at 10 Hz: under 10 % of a core | gcc | 1.17 % | met | 2.02 |
| 16 devices on one io_runner thread, real sockets and timers, a whole frame per write, 16 bytes at 25 Hz: under 10 % of a core | clang | 2.33 % | met | 2.21 |
| 16 devices on one io_runner thread, real sockets and timers, a whole frame per write, 16 bytes at 25 Hz: under 10 % of a core | gcc | 2.46 % | met | 1.87 |
| 16 devices on one io_runner thread, real sockets and timers, a whole frame per write, 16 bytes at 20 Hz: under 10 % of a core | clang | 1.65 % | met | 1.97 |
| 16 devices on one io_runner thread, real sockets and timers, a whole frame per write, 16 bytes at 20 Hz: under 10 % of a core | gcc | 1.85 % | met | 1.53 |
| Compile time of device_m1_20_methods (20 called rows), syntax: under 0.1 s per called row, peak memory under 1500 MB | gcc | 0.047 s per row, 528 MB | met | 2.03 |
| Compile time of device_m1_20_methods (20 called rows), syntax: under 0.1 s per called row, peak memory under 1500 MB | clang | 0.044 s per row, 393 MB | met | 1.07 |
| Compile time of device_m1_20_methods (20 called rows), O0: under 1.0 s per called row, peak memory under 1500 MB | gcc | 0.242 s per row, 771 MB | met | 1.53 |
| Compile time of device_m1_20_methods (20 called rows), O0: under 1.0 s per called row, peak memory under 1500 MB | clang | 0.116 s per row, 513 MB | met | 1.04 |
| Compile time of device_m1_20_methods (20 called rows), O2: under 1.0 s per called row, peak memory under 1500 MB | gcc | 0.497 s per row, 997 MB | met | 1.12 |
| Compile time of device_m1_20_methods (20 called rows), O2: under 1.0 s per called row, peak memory under 1500 MB | clang | 0.617 s per row, 1205 MB | met | 1.59 |
| Compile time of device_m2_15_methods (15 called rows), syntax: under 0.1 s per called row, peak memory under 1500 MB | gcc | 0.035 s per row, 475 MB | met | 1.95 |
| Compile time of device_m2_15_methods (15 called rows), syntax: under 0.1 s per called row, peak memory under 1500 MB | clang | 0.043 s per row, 362 MB | met | 1.06 |
| Compile time of device_m2_15_methods (15 called rows), O0: under 1.0 s per called row, peak memory under 1500 MB | gcc | 0.236 s per row, 700 MB | met | 1.41 |
| Compile time of device_m2_15_methods (15 called rows), O0: under 1.0 s per called row, peak memory under 1500 MB | clang | 0.116 s per row, 454 MB | met | 1.03 |
| Compile time of device_m2_15_methods (15 called rows), O2: under 1.0 s per called row, peak memory under 1500 MB | gcc | 0.510 s per row, 803 MB | met | 1.08 |
| Compile time of device_m2_15_methods (15 called rows), O2: under 1.0 s per called row, peak memory under 1500 MB | clang | 0.569 s per row, 766 MB | met | 1.87 |
| Compile time of device_m1_sync_20_methods (20 called rows), syntax: under 0.1 s per called row, peak memory under 1500 MB | gcc | 0.046 s per row, 518 MB | met | 1.87 |
| Compile time of device_m1_sync_20_methods (20 called rows), syntax: under 0.1 s per called row, peak memory under 1500 MB | clang | 0.042 s per row, 385 MB | met | 1.06 |
| Compile time of device_m1_sync_20_methods (20 called rows), O0: under 1.0 s per called row, peak memory under 1500 MB | gcc | 0.252 s per row, 778 MB | met | 1.29 |
| Compile time of device_m1_sync_20_methods (20 called rows), O0: under 1.0 s per called row, peak memory under 1500 MB | clang | 0.119 s per row, 511 MB | met | 1.03 |
| Compile time of device_m1_sync_20_methods (20 called rows), O2: under 1.0 s per called row, peak memory under 1500 MB | gcc | 0.572 s per row, 1029 MB | met | 1.09 |
| Compile time of device_m1_sync_20_methods (20 called rows), O2: under 1.0 s per called row, peak memory under 1500 MB | clang | 0.402 s per row, 527 MB | met | 1.99 |

## codec

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| slip_16_encode | none | ns/frame | 10.68 | 10.64 | 1.73 | 12.95 | 12.94 | 2.03 |
| slip_16_decode | none | ns/frame | 40.42 | 40.21 | 1.73 | 27.32 | 26.58 | 2.03 |
| slip_16_decode_chunks64 | none | ns/frame | 40.82 | 40.33 | 1.75 | 23.19 | 22.06 | 2.03 |
| cobs_16_encode | none | ns/frame | 24.52 | 22.15 | 1.75 | 42.46 | 42.10 | 2.03 |
| cobs_16_decode | none | ns/frame | 44.83 | 44.75 | 1.75 | 19.05 | 18.65 | 2.03 |
| cobs_16_decode_chunks64 | none | ns/frame | 45.37 | 45.32 | 1.75 | 20.09 | 20.02 | 2.03 |
| delimiter_16_encode | none | ns/frame | 4.98 | 4.96 | 1.75 | 4.92 | 4.85 | 2.03 |
| delimiter_16_decode | none | ns/frame | 44.78 | 44.33 | 1.75 | 28.94 | 27.91 | 2.03 |
| delimiter_16_decode_chunks64 | none | ns/frame | 44.26 | 44.12 | 1.75 | 30.67 | 30.23 | 2.03 |
| length_prefix_16_encode | none | ns/frame | 7.72 | 7.43 | 1.75 | 8.60 | 8.36 | 2.03 |
| length_prefix_16_decode | none | ns/frame | 41.86 | 41.81 | 1.75 | 25.03 | 23.61 | 2.03 |
| length_prefix_16_decode_chunks64 | none | ns/frame | 42.19 | 41.66 | 1.77 | 30.28 | 28.86 | 2.03 |
| header_16_encode | none | ns/frame | 16.32 | 15.93 | 1.77 | 13.56 | 12.81 | 2.03 |
| header_16_decode | none | ns/frame | 19.01 | 18.56 | 1.77 | 16.74 | 16.28 | 2.03 |
| header_16_decode_chunks64 | none | ns/frame | 19.01 | 18.30 | 1.77 | 17.42 | 17.29 | 2.03 |
| modbus_rtu_16_encode | none | ns/frame | 13.72 | 13.66 | 1.77 | 28.41 | 27.64 | 2.03 |
| modbus_rtu_16_decode | none | ns/frame | 20.46 | 20.37 | 1.77 | 31.98 | 31.92 | 2.03 |
| modbus_rtu_16_decode_chunks64 | none | ns/frame | 21.17 | 21.11 | 1.77 | 32.88 | 32.17 | 2.03 |
| slip_100_encode | none | ns/frame | 64.97 | 59.52 | 1.77 | 74.39 | 72.70 | 2.03 |
| slip_100_decode | none | ns/frame | 189.46 | 188.15 | 1.77 | 118.26 | 117.57 | 2.02 |
| slip_100_decode_chunks64 | none | ns/frame | 192.83 | 191.92 | 1.79 | 129.03 | 127.82 | 2.02 |
| cobs_100_encode | none | ns/frame | 94.84 | 93.86 | 1.79 | 221.85 | 221.38 | 2.02 |
| cobs_100_decode | none | ns/frame | 189.41 | 188.86 | 1.79 | 98.14 | 97.69 | 2.02 |
| cobs_100_decode_chunks64 | none | ns/frame | 198.72 | 198.22 | 1.79 | 104.08 | 101.91 | 2.02 |
| delimiter_100_encode | none | ns/frame | 5.62 | 5.62 | 1.79 | 5.40 | 5.38 | 2.02 |
| delimiter_100_decode | none | ns/frame | 202.04 | 201.66 | 1.79 | 120.62 | 119.74 | 2.02 |
| delimiter_100_decode_chunks64 | none | ns/frame | 215.44 | 213.19 | 1.79 | 131.28 | 129.04 | 2.02 |
| length_prefix_100_encode | none | ns/frame | 7.67 | 7.51 | 1.79 | 8.55 | 8.28 | 2.02 |
| length_prefix_100_decode | none | ns/frame | 193.63 | 190.16 | 1.79 | 107.03 | 106.85 | 2.02 |
| length_prefix_100_decode_chunks64 | none | ns/frame | 203.27 | 200.49 | 1.79 | 122.38 | 121.04 | 2.02 |
| header_100_encode | none | ns/frame | 15.75 | 15.58 | 1.81 | 13.03 | 12.80 | 2.02 |
| header_100_decode | none | ns/frame | 19.00 | 17.82 | 1.81 | 16.79 | 16.49 | 2.02 |
| header_100_decode_chunks64 | none | ns/frame | 29.37 | 28.35 | 1.81 | 24.31 | 24.00 | 2.02 |
| modbus_rtu_100_encode | none | ns/frame | 140.16 | 139.94 | 1.81 | 195.42 | 195.24 | 2.02 |
| modbus_rtu_100_decode | none | ns/frame | 160.95 | 160.72 | 1.81 | 206.11 | 206.04 | 2.02 |
| modbus_rtu_100_decode_chunks64 | none | ns/frame | 173.08 | 172.97 | 1.81 | 218.99 | 218.90 | 2.02 |
| slip_2420_encode | none | ns/frame | 1258.79 | 1238.28 | 1.81 | 1841.36 | 1795.70 | 2.02 |
| slip_2420_decode | none | ns/frame | 4200.34 | 4180.81 | 1.81 | 2500.89 | 2495.85 | 2.02 |
| slip_2420_decode_chunks64 | none | ns/frame | 4609.41 | 4581.87 | 1.81 | 2958.58 | 2940.32 | 2.10 |
| cobs_2420_encode | none | ns/frame | 2030.34 | 2005.44 | 1.82 | 5235.27 | 5215.47 | 2.10 |
| cobs_2420_decode | none | ns/frame | 4280.02 | 4136.19 | 1.82 | 2066.01 | 2042.84 | 2.10 |
| cobs_2420_decode_chunks64 | none | ns/frame | 4445.47 | 4432.85 | 1.82 | 2284.04 | 2277.13 | 2.10 |
| delimiter_2420_encode | none | ns/frame | 18.97 | 18.96 | 1.82 | 23.25 | 23.25 | 2.10 |
| delimiter_2420_decode | none | ns/frame | 4555.85 | 4549.69 | 1.82 | 2557.20 | 2516.80 | 2.10 |
| delimiter_2420_decode_chunks64 | none | ns/frame | 4944.90 | 4906.74 | 1.82 | 2913.83 | 2898.81 | 2.10 |
| length_prefix_2420_encode | none | ns/frame | 21.07 | 21.05 | 1.82 | 29.13 | 29.06 | 2.10 |
| length_prefix_2420_decode | none | ns/frame | 4395.21 | 4339.79 | 1.82 | 2308.26 | 2287.18 | 2.10 |
| length_prefix_2420_decode_chunks64 | none | ns/frame | 4695.83 | 4667.52 | 1.82 | 2717.71 | 2707.09 | 2.09 |
| header_2420_encode | none | ns/frame | 24.69 | 24.47 | 2.00 | 31.39 | 31.32 | 2.09 |
| header_2420_decode | none | ns/frame | 39.19 | 39.09 | 2.00 | 48.59 | 48.55 | 2.09 |
| header_2420_decode_chunks64 | none | ns/frame | 359.93 | 358.53 | 2.00 | 290.63 | 288.35 | 2.09 |

- `slip_16_encode`: MB/s=1498
- `slip_16_decode`: MB/s=395
- `slip_16_decode_chunks64`: MB/s=391
- `cobs_16_encode`: MB/s=652
- `cobs_16_decode`: MB/s=356
- `cobs_16_decode_chunks64`: MB/s=352
- `delimiter_16_encode`: MB/s=3213
- `delimiter_16_decode`: MB/s=357
- `delimiter_16_decode_chunks64`: MB/s=361
- `length_prefix_16_encode`: MB/s=2073
- `length_prefix_16_decode`: MB/s=382
- `length_prefix_16_decode_chunks64`: MB/s=379
- `header_16_encode`: MB/s=980
- `header_16_decode`: MB/s=841
- `header_16_decode_chunks64`: MB/s=841
- `modbus_rtu_16_encode`: MB/s=1166
- `modbus_rtu_16_decode`: MB/s=782
- `modbus_rtu_16_decode_chunks64`: MB/s=755
- `slip_100_encode`: MB/s=1539
- `slip_100_decode`: MB/s=527
- `slip_100_decode_chunks64`: MB/s=518
- `cobs_100_encode`: MB/s=1054
- `cobs_100_decode`: MB/s=527
- `cobs_100_decode_chunks64`: MB/s=503
- `delimiter_100_encode`: MB/s=17778
- `delimiter_100_decode`: MB/s=494
- `delimiter_100_decode_chunks64`: MB/s=464
- `length_prefix_100_encode`: MB/s=13033
- `length_prefix_100_decode`: MB/s=516
- `length_prefix_100_decode_chunks64`: MB/s=491
- `header_100_encode`: MB/s=6347
- `header_100_decode`: MB/s=5264
- `header_100_decode_chunks64`: MB/s=3405
- `modbus_rtu_100_encode`: MB/s=713
- `modbus_rtu_100_decode`: MB/s=621
- `modbus_rtu_100_decode_chunks64`: MB/s=577
- `slip_2420_encode`: MB/s=1922
- `slip_2420_decode`: MB/s=576
- `slip_2420_decode_chunks64`: MB/s=525
- `cobs_2420_encode`: MB/s=1191
- `cobs_2420_decode`: MB/s=565
- `cobs_2420_decode_chunks64`: MB/s=544
- `delimiter_2420_encode`: MB/s=127560
- `delimiter_2420_decode`: MB/s=531
- `delimiter_2420_decode_chunks64`: MB/s=489
- `length_prefix_2420_encode`: MB/s=114854
- `length_prefix_2420_decode`: MB/s=550
- `length_prefix_2420_decode_chunks64`: MB/s=515
- `header_2420_encode`: MB/s=98024
- `header_2420_decode`: MB/s=61751
- `header_2420_decode_chunks64`: MB/s=6723

## protocol

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| message_decode_100 | none | ns/message | 32.23 | 32.09 | 2.00 | 85.90 | 84.67 | 2.12 |
| message_encode_100 | none | ns/message | 136.58 | 135.81 | 2.00 | 185.00 | 184.34 | 2.12 |
| index_match_100 | none | ns/frame | 7.49 | 7.47 | 2.00 | 8.88 | 8.87 | 2.12 |
| describe_rx_100 | none | ns/frame | 878.02 | 873.95 | 2.00 | 869.66 | 867.11 | 2.12 |
| message_decode_2420 | none | ns/message | 66.00 | 64.12 | 2.00 | 320.79 | 319.11 | 2.12 |
| message_encode_2420 | none | ns/message | 3736.43 | 3698.21 | 2.00 | 3523.61 | 3395.56 | 2.12 |
| index_match_2420 | none | ns/frame | 7.48 | 7.46 | 2.00 | 8.89 | 8.87 | 2.12 |
| describe_rx_2420 | none | ns/frame | 13124.84 | 13103.25 | 2.00 | 12475.13 | 12460.47 | 2.12 |
| message_decode_6 | none | ns/message | 1.01 | 1.00 | 2.00 | 2.75 | 2.74 | 2.11 |
| message_encode_6 | none | ns/message | 11.24 | 11.04 | 2.00 | 10.61 | 10.39 | 2.11 |
| index_match_6 | none | ns/frame | 7.55 | 7.48 | 2.00 | 8.89 | 8.88 | 2.11 |
| describe_rx_6 | none | ns/frame | 148.90 | 148.33 | 2.00 | 152.46 | 151.92 | 2.11 |
| describe_tx_6 | none | ns/frame | 150.00 | 149.45 | 2.00 | 149.33 | 149.08 | 2.11 |

- `message_decode_100`: bytes=100
- `message_encode_100`: bytes=100
- `index_match_100`: bytes=100
- `describe_rx_100`: bytes=100 chars=199
- `message_decode_2420`: bytes=2420
- `message_encode_2420`: bytes=2420
- `index_match_2420`: bytes=2420
- `describe_rx_2420`: bytes=2420 chars=3688
- `message_decode_6`: bytes=6
- `message_encode_6`: bytes=6
- `index_match_6`: bytes=6
- `describe_rx_6`: bytes=6 chars=54
- `describe_tx_6`: bytes=6

## logging

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| lazy_below_threshold | none | ns/call | 0.86 | 0.86 | 2.00 | 0.94 | 0.94 | 1.42 |
| eager_below_threshold | none | ns/call | 53.19 | 53.10 | 2.00 | 54.18 | 53.21 | 1.42 |
| bare_atomic_load | none | ns/call | 0.12 | 0.12 | 2.00 | 0.29 | 0.29 | 1.42 |
| filtered_component | none | ns/call | 12.89 | 12.83 | 2.00 | 12.77 | 12.73 | 1.42 |
| enabled_counting_sink | none | ns/call | 55.32 | 54.98 | 2.00 | 53.47 | 53.42 | 1.42 |

- `lazy_below_threshold`: the macro; threshold above the record
- `eager_below_threshold`: falsifier: formats before the check
- `bare_atomic_load`: an atomic load and a branch
- `filtered_component`: level passes; component denied
- `enabled_counting_sink`: the record is built and handed to a counting sink; records=8010101

## dialogue

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ping6_virtual | virtual | ns/exchange | 6280.84 | 6263.73 | 1.83 | 6131.61 | 6129.83 | 1.42 |
| ping6_virtual_device_sleep50us | virtual | ns/exchange | 140017.47 | 122598.09 | 1.83 | 122020.17 | 110582.26 | 1.42 |
| block2420_virtual | virtual | ns/exchange | 19185.96 | 18897.04 | 1.83 | 14945.74 | 14914.10 | 1.42 |
| block2420_virtual_device_sleep50us | virtual | ns/exchange | 135522.38 | 126884.97 | 1.83 | 163812.11 | 141696.24 | 1.42 |
| ping6_real_timers | real | ns/exchange | 9758.01 | 9726.98 | 1.83 | 9306.36 | 9291.27 | 1.55 |
| ping6_real_timers_device_sleep50us | real | ns/exchange | 133284.26 | 111890.40 | 1.76 | 118710.11 | 113600.17 | 1.55 |
| ping6_sync_thread | real | ns/exchange | 14870.89 | 14488.28 | 1.76 | 15375.20 | 14288.17 | 1.55 |
| ping6_sync_thread_device_sleep50us | real | ns/exchange | 158516.91 | 121399.47 | 1.76 | 142558.87 | 117658.58 | 1.55 |
| block2420_sync_thread | real | ns/exchange | 29187.91 | 28238.60 | 1.76 | 26858.03 | 24583.04 | 1.55 |
| block2420_sync_thread_device_sleep50us | real | ns/exchange | 167856.51 | 135883.89 | 1.76 | 163417.83 | 127560.30 | 1.55 |
| ping6_sync_tcp_loopback | real | ns/exchange | 25206.76 | 23519.70 | 1.76 | 27254.14 | 20950.34 | 1.55 |
| ping6_sync_tcp_loopback_device_sleep50us | real | ns/exchange | 186880.67 | 148673.98 | 1.76 | 137663.65 | 131705.59 | 1.55 |

- `ping6_virtual`: async; 6-byte reply
- `ping6_virtual_device_sleep50us`: falsifier: sleep_for (50 us) in the simulator handler
- `block2420_virtual`: async; 2420-byte reply
- `block2420_virtual_device_sleep50us`: falsifier: sleep_for (50 us) in the simulator handler
- `ping6_real_timers`: async; 6-byte reply
- `ping6_real_timers_device_sleep50us`: falsifier: sleep_for (50 us) in the simulator handler
- `ping6_sync_thread`: sync call; the context runs on another thread
- `ping6_sync_thread_device_sleep50us`: falsifier: sleep_for (50 us) in the simulator handler
- `block2420_sync_thread`: sync call; the context runs on another thread
- `block2420_sync_thread_device_sleep50us`: falsifier: sleep_for (50 us) in the simulator handler
- `ping6_sync_tcp_loopback`: sync call over TCP on the loopback interface; the client on an io_runner with real timers
- `ping6_sync_tcp_loopback_device_sleep50us`: falsifier: sleep_for (50 us) in the peer thread

## devices (CPU on mocks, virtual clock: no budget verdict)

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| sized_10_with_clients | virtual | busy_%_of_a_core | 0.26 | 0.26 | 1.76 | 0.20 | 0.19 | 1.55 |
| sized_10_devices_only | virtual | busy_%_of_a_core | 0.13 | 0.13 | 1.76 | 0.13 | 0.13 | 1.55 |
| sized_25_with_clients | virtual | busy_%_of_a_core | 0.64 | 0.64 | 1.76 | 0.49 | 0.49 | 1.59 |
| sized_25_devices_only | virtual | busy_%_of_a_core | 0.32 | 0.32 | 1.78 | 0.32 | 0.32 | 1.59 |
| small_20_with_clients | virtual | busy_%_of_a_core | 0.12 | 0.12 | 1.78 | 0.11 | 0.11 | 1.59 |
| small_20_devices_only | virtual | busy_%_of_a_core | 0.07 | 0.07 | 1.78 | 0.07 | 0.07 | 1.59 |
| small_25_with_clients | virtual | busy_%_of_a_core | 0.15 | 0.15 | 1.78 | 0.14 | 0.14 | 1.59 |
| small_25_devices_only | virtual | busy_%_of_a_core | 0.09 | 0.09 | 1.78 | 0.09 | 0.09 | 1.59 |
| mixed_with_clients | virtual | busy_%_of_a_core | 0.18 | 0.18 | 1.78 | 0.15 | 0.15 | 1.59 |
| mixed_devices_only | virtual | busy_%_of_a_core | 0.10 | 0.10 | 1.78 | 0.10 | 0.10 | 1.59 |

- `sized_10_with_clients`: CPU on mocks; messages=1600 per 10 virtual s; x388 faster than real time; cpu 0.240041 %
- `sized_10_devices_only`: CPU on mocks; bytes=3855824 per 10 virtual s; x770 faster than real time; cpu 0.117625 %
- `sized_25_with_clients`: CPU on mocks; messages=4000 per 10 virtual s; x155 faster than real time; cpu 0.640029 %
- `sized_25_devices_only`: CPU on mocks; bytes=9663780 per 10 virtual s; x309 faster than real time; cpu 0.295985 %
- `small_20_with_clients`: CPU on mocks; messages=3200 per 10 virtual s; x820 faster than real time; cpu 0.120044 %
- `small_20_devices_only`: CPU on mocks; bytes=57402 per 10 virtual s; x1382 faster than real time; cpu 0.050425 %
- `small_25_with_clients`: CPU on mocks; messages=4000 per 10 virtual s; x654 faster than real time; cpu 0.160040 %
- `small_25_devices_only`: CPU on mocks; bytes=71874 per 10 virtual s; x1106 faster than real time; cpu 0.070766 %
- `mixed_with_clients`: CPU on mocks; messages=2850 per 10 virtual s; x542 faster than real time; cpu 0.189078 %
- `mixed_devices_only`: CPU on mocks; bytes=1493538 per 10 virtual s; x1013 faster than real time; cpu 0.076959 %

## sockets (16 devices over TCP loopback, real timers; the chunked rows have no budget)

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| sized_25_whole | real | busy_%_of_a_core | 3.15 | 3.02 | 1.68 | 2.52 | 2.51 | 1.98 |
| sized_10_whole | real | busy_%_of_a_core | 1.35 | 1.28 | 2.03 | 1.17 | 1.17 | 2.02 |
| small_25_whole | real | busy_%_of_a_core | 2.33 | 1.87 | 2.21 | 2.46 | 2.26 | 1.87 |
| small_20_whole | real | busy_%_of_a_core | 1.65 | 1.61 | 1.97 | 1.85 | 1.78 | 1.53 |
| sized_25_chunks64 | real | busy_%_of_a_core | 23.86 | 22.35 | 2.25 | 23.12 | 19.69 | 2.68 |
| sized_10_chunks64 | real | busy_%_of_a_core | 11.33 | 10.74 | 2.38 | 12.80 | 11.37 | 2.59 |

- `sized_25_whole`: 16 clients on one io_runner thread over TCP loopback; 400.1 of 400 messages/s (100 %); 78.6 us of CPU per message; 577 voluntary context switches/s
- `sized_10_whole`: 16 clients on one io_runner thread over TCP loopback; 160.0 of 160 messages/s (100 %); 84.6 us of CPU per message; 227 voluntary context switches/s
- `small_25_whole`: 16 clients on one io_runner thread over TCP loopback; 400.0 of 400 messages/s (100 %); 58.3 us of CPU per message; 585 voluntary context switches/s
- `small_20_whole`: 16 clients on one io_runner thread over TCP loopback; 320.1 of 320 messages/s (100 %); 51.5 us of CPU per message; 477 voluntary context switches/s
- `sized_25_chunks64`: 16 clients on one io_runner thread over TCP loopback; 347.7 of 400 messages/s (87 %); 686.3 us of CPU per message; 1098 voluntary context switches/s
- `sized_10_chunks64`: 16 clients on one io_runner thread over TCP loopback; 160.0 of 160 messages/s (100 %); 708.0 us of CPU per message; 992 voluntary context switches/s

## Compile time of protocol::client<P>

- compiler gcc: g++ (GCC) 13.2.0
- compiler clang: clang version 23.1.0

| Toolchain | Mode | Variant | Called rows | Median s | Min s | Seconds per called row (over headers) | Peak MB | Load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| gcc | O0 | device_m1_20_methods | 20 | 6.31 | 6.29 | 0.242 | 771 | 1.53 |
| gcc | O0 | device_m1_sync_20_methods | 20 | 6.51 | 6.51 | 0.252 | 778 | 1.29 |
| gcc | O0 | device_m2_15_methods | 15 | 5.01 | 4.99 | 0.236 | 700 | 1.41 |
| gcc | O0 | headers | - | 1.47 | 1.47 | - | 385 | 1.80 |
| gcc | O0 | table7_one_method | 1 | 2.64 | 2.64 | 1.170 | 500 | 1.68 |
| gcc | O2 | device_m1_20_methods | 20 | 11.51 | 11.50 | 0.497 | 997 | 1.12 |
| gcc | O2 | device_m1_sync_20_methods | 20 | 12.99 | 12.99 | 0.572 | 1029 | 1.09 |
| gcc | O2 | device_m2_15_methods | 15 | 9.21 | 9.20 | 0.510 | 803 | 1.08 |
| gcc | O2 | headers | - | 1.56 | 1.55 | - | 395 | 1.27 |
| gcc | O2 | table7_one_method | 1 | 4.44 | 4.41 | 2.880 | 521 | 1.21 |
| gcc | syntax | device_m1_20_methods | 20 | 2.22 | 2.22 | 0.047 | 528 | 2.03 |
| gcc | syntax | device_m1_sync_20_methods | 20 | 2.21 | 2.20 | 0.046 | 518 | 1.87 |
| gcc | syntax | device_m2_15_methods | 15 | 1.80 | 1.80 | 0.035 | 475 | 1.95 |
| gcc | syntax | headers | - | 1.28 | 1.27 | - | 358 | 2.22 |
| gcc | syntax | table7_one_method | 1 | 1.40 | 1.40 | 0.120 | 389 | 2.22 |
| clang | O0 | device_m1_20_methods | 20 | 3.63 | 3.57 | 0.116 | 513 | 1.04 |
| clang | O0 | device_m1_sync_20_methods | 20 | 3.68 | 3.61 | 0.119 | 511 | 1.03 |
| clang | O0 | device_m2_15_methods | 15 | 3.05 | 3.00 | 0.116 | 454 | 1.03 |
| clang | O0 | headers | - | 1.31 | 1.31 | - | 274 | 1.05 |
| clang | O0 | table7_one_method | 1 | 1.85 | 1.84 | 0.540 | 328 | 1.05 |
| clang | O2 | device_m1_20_methods | 20 | 13.68 | 13.64 | 0.617 | 1205 | 1.59 |
| clang | O2 | device_m1_sync_20_methods | 20 | 9.39 | 9.27 | 0.402 | 527 | 1.99 |
| clang | O2 | device_m2_15_methods | 15 | 9.89 | 9.83 | 0.569 | 766 | 1.87 |
| clang | O2 | headers | - | 1.35 | 1.34 | - | 274 | 1.03 |
| clang | O2 | table7_one_method | 1 | 3.09 | 3.07 | 1.740 | 334 | 1.02 |
| clang | syntax | device_m1_20_methods | 20 | 2.10 | 2.09 | 0.044 | 393 | 1.07 |
| clang | syntax | device_m1_sync_20_methods | 20 | 2.07 | 2.06 | 0.042 | 385 | 1.06 |
| clang | syntax | device_m2_15_methods | 15 | 1.88 | 1.87 | 0.043 | 362 | 1.06 |
| clang | syntax | headers | - | 1.23 | 1.22 | - | 267 | 1.08 |
| clang | syntax | table7_one_method | 1 | 1.36 | 1.35 | 0.130 | 285 | 1.08 |

