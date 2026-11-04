# Transport benchmarks

- clang: machine="12th Gen Intel(R) Core(TM) i7-12700K" logical_cpus=20; compiler="Clang 23.1.0" cplusplus=201103; build=NDEBUG
- gcc: machine="12th Gen Intel(R) Core(TM) i7-12700K" logical_cpus=20; compiler="GCC 13.2.0" cplusplus=201103; build=NDEBUG

## Budgets

| Budget | Toolchain | Measured | Verdict | Load |
| --- | --- | --- | --- | --- |
| Log call below the threshold: under 2 ns | clang | 0.86 ns | met | 1.78 |
| Log call below the threshold: under 2 ns | gcc | 0.94 ns | met | 1.85 |
| Dialogue round trip on mocks (async, virtual clock): under 50 us | clang | 6.3 us | met | 1.78 |
| Dialogue round trip on mocks (async, virtual clock): under 50 us | gcc | 6.1 us | met | 1.85 |
| Dialogue round trip on mocks (sync call, runner thread): under 50 us | clang | 14.8 us | met | 1.80 |
| Dialogue round trip on mocks (sync call, runner thread): under 50 us | gcc | 14.9 us | met | 1.86 |
| describe of a 100-byte frame: under 5 us | clang | 0.88 us | met | 1.64 |
| describe of a 100-byte frame: under 5 us | gcc | 0.87 us | met | 1.75 |
| Codecs at 2420 bytes: at least 100 MB/s (the slowest shown) | clang | delimiter_2420_decode at 531 MB/s | met | 1.93 |
| Codecs at 2420 bytes: at least 100 MB/s (the slowest shown) | gcc | cobs_2420_encode at 464 MB/s | met | 1.70 |
| 16 devices on one io_runner thread, 2420 bytes at 25 Hz: under 10 % of a core | clang | 0.64 % | met | 1.80 |
| 16 devices on one io_runner thread, 2420 bytes at 25 Hz: under 10 % of a core | gcc | 0.48 % | met | 1.86 |
| 16 devices on one io_runner thread, 16 bytes at 25 Hz: under 10 % of a core | clang | 0.15 % | met | 1.80 |
| 16 devices on one io_runner thread, 16 bytes at 25 Hz: under 10 % of a core | gcc | 0.14 % | met | 1.87 |
| 16 devices on one io_runner thread, 16 bytes at 20 Hz: under 10 % of a core | clang | 0.12 % | met | 1.80 |
| 16 devices on one io_runner thread, 16 bytes at 20 Hz: under 10 % of a core | gcc | 0.11 % | met | 1.86 |
| 16 devices on one io_runner thread, mixed loads: under 10 % of a core | clang | 0.18 % | met | 1.80 |
| 16 devices on one io_runner thread, mixed loads: under 10 % of a core | gcc | 0.15 % | met | 1.87 |
| Client over the largest table adds under 2 s (gcc, syntax) | gcc | +1.06 s, 528 MB | met | 2.05 |
| Client over the largest table adds under 2 s (gcc, O0) | gcc | +5.51 s, 771 MB | MISSED | 2.44 |
| Client over the largest table adds under 2 s (gcc, O2) | gcc | +11.35 s, 997 MB | MISSED | 2.93 |
| Client over the largest table adds under 2 s (clang, syntax) | clang | +1.02 s, 389 MB | met | 2.72 |
| Client over the largest table adds under 2 s (clang, O0) | clang | +2.64 s, 510 MB | MISSED | 2.49 |
| Client over the largest table adds under 2 s (clang, O2) | clang | +14.15 s, 1201 MB | MISSED | 2.92 |

## codec

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| slip_16_encode | none | ns/frame | 10.78 | 10.73 | 1.77 | 14.27 | 13.69 | 1.47 |
| slip_16_decode | none | ns/frame | 41.08 | 40.92 | 1.77 | 23.35 | 22.29 | 1.47 |
| slip_16_decode_chunks64 | none | ns/frame | 41.04 | 40.76 | 1.77 | 27.88 | 26.41 | 1.47 |
| cobs_16_encode | none | ns/frame | 22.30 | 20.91 | 1.79 | 41.72 | 40.95 | 1.47 |
| cobs_16_decode | none | ns/frame | 45.71 | 45.59 | 1.79 | 19.96 | 19.43 | 1.47 |
| cobs_16_decode_chunks64 | none | ns/frame | 46.15 | 46.06 | 1.79 | 19.44 | 19.29 | 1.47 |
| delimiter_16_encode | none | ns/frame | 5.19 | 5.07 | 1.79 | 10.75 | 10.64 | 1.52 |
| delimiter_16_decode | none | ns/frame | 46.52 | 45.63 | 1.79 | 35.16 | 34.97 | 1.52 |
| delimiter_16_decode_chunks64 | none | ns/frame | 47.96 | 46.63 | 1.79 | 30.31 | 30.06 | 1.52 |
| length_prefix_16_encode | none | ns/frame | 7.79 | 7.55 | 1.79 | 8.77 | 8.60 | 1.52 |
| length_prefix_16_decode | none | ns/frame | 45.34 | 44.37 | 1.79 | 29.60 | 29.21 | 1.52 |
| length_prefix_16_decode_chunks64 | none | ns/frame | 45.29 | 44.62 | 1.79 | 29.14 | 27.92 | 1.52 |
| header_16_encode | none | ns/frame | 13.85 | 13.06 | 1.81 | 17.79 | 17.71 | 1.52 |
| header_16_decode | none | ns/frame | 20.02 | 18.30 | 1.81 | 16.70 | 15.94 | 1.52 |
| header_16_decode_chunks64 | none | ns/frame | 19.92 | 19.44 | 1.81 | 16.90 | 16.12 | 1.52 |
| modbus_rtu_16_encode | none | ns/frame | 13.70 | 13.49 | 1.81 | 28.25 | 27.94 | 1.52 |
| modbus_rtu_16_decode | none | ns/frame | 21.51 | 21.09 | 1.81 | 31.68 | 31.54 | 1.56 |
| modbus_rtu_16_decode_chunks64 | none | ns/frame | 21.03 | 20.76 | 1.81 | 32.92 | 32.65 | 1.56 |
| slip_100_encode | none | ns/frame | 66.67 | 64.32 | 1.81 | 75.16 | 72.86 | 1.56 |
| slip_100_decode | none | ns/frame | 189.82 | 189.54 | 1.81 | 130.24 | 129.96 | 1.56 |
| slip_100_decode_chunks64 | none | ns/frame | 193.01 | 192.24 | 1.81 | 127.66 | 126.45 | 1.56 |
| cobs_100_encode | none | ns/frame | 94.36 | 93.70 | 1.83 | 218.59 | 218.08 | 1.56 |
| cobs_100_decode | none | ns/frame | 190.28 | 189.44 | 1.83 | 99.66 | 99.03 | 1.56 |
| cobs_100_decode_chunks64 | none | ns/frame | 200.84 | 199.85 | 1.83 | 100.66 | 98.56 | 1.56 |
| delimiter_100_encode | none | ns/frame | 5.66 | 5.61 | 1.83 | 10.63 | 10.33 | 1.56 |
| delimiter_100_decode | none | ns/frame | 206.06 | 203.12 | 1.83 | 141.87 | 140.69 | 1.59 |
| delimiter_100_decode_chunks64 | none | ns/frame | 214.02 | 213.72 | 1.83 | 130.51 | 129.89 | 1.59 |
| length_prefix_100_encode | none | ns/frame | 7.90 | 7.79 | 1.83 | 9.25 | 9.18 | 1.59 |
| length_prefix_100_decode | none | ns/frame | 194.94 | 194.25 | 1.83 | 108.14 | 107.86 | 1.59 |
| length_prefix_100_decode_chunks64 | none | ns/frame | 204.86 | 204.67 | 1.83 | 120.91 | 120.16 | 1.59 |
| header_100_encode | none | ns/frame | 13.94 | 13.22 | 1.83 | 17.20 | 16.78 | 1.59 |
| header_100_decode | none | ns/frame | 19.42 | 18.19 | 1.84 | 16.68 | 16.57 | 1.59 |
| header_100_decode_chunks64 | none | ns/frame | 28.20 | 27.44 | 1.84 | 25.70 | 25.21 | 1.59 |
| modbus_rtu_100_encode | none | ns/frame | 140.66 | 140.55 | 1.84 | 194.67 | 194.33 | 1.59 |
| modbus_rtu_100_decode | none | ns/frame | 162.10 | 161.69 | 1.84 | 209.62 | 209.33 | 1.70 |
| modbus_rtu_100_decode_chunks64 | none | ns/frame | 174.57 | 174.00 | 1.84 | 219.66 | 219.55 | 1.70 |
| slip_2420_encode | none | ns/frame | 1286.34 | 1262.79 | 1.84 | 2228.84 | 2153.76 | 1.70 |
| slip_2420_decode | none | ns/frame | 4190.04 | 4186.68 | 1.84 | 2545.89 | 2543.35 | 1.70 |
| slip_2420_decode_chunks64 | none | ns/frame | 4563.73 | 4541.91 | 1.84 | 2915.05 | 2905.77 | 1.70 |
| cobs_2420_encode | none | ns/frame | 1938.77 | 1911.09 | 1.84 | 5212.16 | 5205.69 | 1.70 |
| cobs_2420_decode | none | ns/frame | 4204.81 | 4128.01 | 1.93 | 2616.27 | 2543.30 | 1.70 |
| cobs_2420_decode_chunks64 | none | ns/frame | 4480.13 | 4472.54 | 1.93 | 2842.05 | 2657.51 | 1.70 |
| delimiter_2420_encode | none | ns/frame | 24.12 | 24.09 | 1.93 | 24.50 | 24.38 | 1.70 |
| delimiter_2420_decode | none | ns/frame | 4556.73 | 4544.28 | 1.93 | 3056.77 | 3019.24 | 1.73 |
| delimiter_2420_decode_chunks64 | none | ns/frame | 4873.48 | 4844.85 | 1.93 | 2913.31 | 2877.74 | 1.73 |
| length_prefix_2420_encode | none | ns/frame | 26.85 | 26.82 | 1.93 | 29.04 | 28.82 | 1.73 |
| length_prefix_2420_decode | none | ns/frame | 4289.24 | 4271.07 | 1.93 | 2285.77 | 2281.73 | 1.73 |
| length_prefix_2420_decode_chunks64 | none | ns/frame | 4582.78 | 4573.00 | 1.93 | 2645.47 | 2631.22 | 1.73 |
| header_2420_encode | none | ns/frame | 29.80 | 29.41 | 1.93 | 32.36 | 32.30 | 1.73 |
| header_2420_decode | none | ns/frame | 50.27 | 50.12 | 1.94 | 48.94 | 48.87 | 1.73 |
| header_2420_decode_chunks64 | none | ns/frame | 380.20 | 379.54 | 1.94 | 321.76 | 319.56 | 1.73 |

- `slip_16_encode`: MB/s=1483
- `slip_16_decode`: MB/s=389
- `slip_16_decode_chunks64`: MB/s=389
- `cobs_16_encode`: MB/s=717
- `cobs_16_decode`: MB/s=350
- `cobs_16_decode_chunks64`: MB/s=346
- `delimiter_16_encode`: MB/s=3081
- `delimiter_16_decode`: MB/s=343
- `delimiter_16_decode_chunks64`: MB/s=333
- `length_prefix_16_encode`: MB/s=2053
- `length_prefix_16_decode`: MB/s=352
- `length_prefix_16_decode_chunks64`: MB/s=353
- `header_16_encode`: MB/s=1155
- `header_16_decode`: MB/s=799
- `header_16_decode_chunks64`: MB/s=803
- `modbus_rtu_16_encode`: MB/s=1168
- `modbus_rtu_16_decode`: MB/s=743
- `modbus_rtu_16_decode_chunks64`: MB/s=760
- `slip_100_encode`: MB/s=1499
- `slip_100_decode`: MB/s=526
- `slip_100_decode_chunks64`: MB/s=518
- `cobs_100_encode`: MB/s=1059
- `cobs_100_decode`: MB/s=525
- `cobs_100_decode_chunks64`: MB/s=497
- `delimiter_100_encode`: MB/s=17683
- `delimiter_100_decode`: MB/s=485
- `delimiter_100_decode_chunks64`: MB/s=467
- `length_prefix_100_encode`: MB/s=12655
- `length_prefix_100_decode`: MB/s=512
- `length_prefix_100_decode_chunks64`: MB/s=488
- `header_100_encode`: MB/s=7171
- `header_100_decode`: MB/s=5148
- `header_100_decode_chunks64`: MB/s=3546
- `modbus_rtu_100_encode`: MB/s=710
- `modbus_rtu_100_decode`: MB/s=616
- `modbus_rtu_100_decode_chunks64`: MB/s=572
- `slip_2420_encode`: MB/s=1881
- `slip_2420_decode`: MB/s=577
- `slip_2420_decode_chunks64`: MB/s=530
- `cobs_2420_encode`: MB/s=1248
- `cobs_2420_decode`: MB/s=575
- `cobs_2420_decode_chunks64`: MB/s=540
- `delimiter_2420_encode`: MB/s=100340
- `delimiter_2420_decode`: MB/s=531
- `delimiter_2420_decode_chunks64`: MB/s=496
- `length_prefix_2420_encode`: MB/s=90128
- `length_prefix_2420_decode`: MB/s=564
- `length_prefix_2420_decode_chunks64`: MB/s=528
- `header_2420_encode`: MB/s=81205
- `header_2420_decode`: MB/s=48135
- `header_2420_decode_chunks64`: MB/s=6365

## protocol

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| message_decode_100 | none | ns/message | 32.34 | 31.95 | 1.64 | 85.41 | 84.02 | 1.73 |
| message_encode_100 | none | ns/message | 137.55 | 137.27 | 1.64 | 180.23 | 177.55 | 1.73 |
| index_match_100 | none | ns/frame | 7.48 | 7.48 | 1.64 | 10.00 | 9.95 | 1.75 |
| describe_rx_100 | none | ns/frame | 878.80 | 875.62 | 1.64 | 869.82 | 865.08 | 1.75 |
| message_decode_2420 | none | ns/message | 69.95 | 69.35 | 1.67 | 323.26 | 322.93 | 1.75 |
| message_encode_2420 | none | ns/message | 3720.85 | 3711.03 | 1.67 | 3503.86 | 3354.99 | 1.75 |
| index_match_2420 | none | ns/frame | 7.47 | 7.46 | 1.67 | 9.56 | 9.56 | 1.75 |
| describe_rx_2420 | none | ns/frame | 13158.99 | 13133.12 | 1.67 | 12814.23 | 12764.69 | 1.75 |
| message_decode_6 | none | ns/message | 1.01 | 0.99 | 1.67 | 2.82 | 2.82 | 1.75 |
| message_encode_6 | none | ns/message | 11.62 | 11.00 | 1.67 | 10.07 | 9.83 | 1.75 |
| index_match_6 | none | ns/frame | 7.47 | 7.46 | 1.67 | 9.57 | 9.56 | 1.75 |
| describe_rx_6 | none | ns/frame | 148.99 | 147.95 | 1.67 | 152.84 | 151.48 | 1.85 |
| describe_tx_6 | none | ns/frame | 150.12 | 149.73 | 1.67 | 150.41 | 149.00 | 1.85 |

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
| lazy_below_threshold | none | ns/call | 0.86 | 0.86 | 1.78 | 0.94 | 0.94 | 1.85 |
| eager_below_threshold | none | ns/call | 53.15 | 52.67 | 1.78 | 53.20 | 52.81 | 1.85 |
| bare_atomic_load | none | ns/call | 0.12 | 0.11 | 1.78 | 0.28 | 0.28 | 1.85 |
| filtered_component | none | ns/call | 12.86 | 12.83 | 1.78 | 12.67 | 12.64 | 1.85 |
| enabled_counting_sink | none | ns/call | 55.21 | 54.95 | 1.78 | 53.73 | 53.57 | 1.85 |

- `lazy_below_threshold`: the macro; threshold above the record
- `eager_below_threshold`: falsifier: formats before the check
- `bare_atomic_load`: an atomic load and a branch
- `filtered_component`: level passes; component denied
- `enabled_counting_sink`: the record is built and handed to a counting sink; records=8010101

## dialogue

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| ping6_virtual | virtual | ns/exchange | 6274.90 | 6259.72 | 1.78 | 6072.00 | 6038.88 | 1.85 |
| ping6_virtual_device_sleep50us | virtual | ns/exchange | 118075.80 | 116945.93 | 1.78 | 126529.36 | 111137.91 | 1.85 |
| block2420_virtual | virtual | ns/exchange | 18783.63 | 18728.86 | 1.78 | 14885.95 | 14798.60 | 1.85 |
| block2420_virtual_device_sleep50us | virtual | ns/exchange | 154166.29 | 128553.51 | 1.78 | 162047.29 | 120131.58 | 1.86 |
| ping6_real_timers | real | ns/exchange | 9548.22 | 9503.12 | 1.78 | 9129.02 | 9115.35 | 1.86 |
| ping6_real_timers_device_sleep50us | real | ns/exchange | 153075.50 | 149951.82 | 1.80 | 128078.24 | 121905.94 | 1.86 |
| ping6_sync_thread | real | ns/exchange | 14838.58 | 14542.84 | 1.80 | 14872.73 | 13811.67 | 1.86 |
| ping6_sync_thread_device_sleep50us | real | ns/exchange | 125840.26 | 122316.12 | 1.80 | 154515.93 | 132351.56 | 1.86 |
| block2420_sync_thread | real | ns/exchange | 35566.27 | 31283.65 | 1.80 | 24888.31 | 24024.76 | 1.86 |
| block2420_sync_thread_device_sleep50us | real | ns/exchange | 169460.23 | 139895.04 | 1.80 | 140892.88 | 133893.80 | 1.86 |

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

## devices

| Case | Clock | Unit | clang median | clang min | clang load | gcc median | gcc min | gcc load |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| sized_10_with_clients | virtual | busy_%_of_a_core | 0.26 | 0.26 | 1.80 | 0.20 | 0.19 | 1.86 |
| sized_10_devices_only | virtual | busy_%_of_a_core | 0.13 | 0.13 | 1.80 | 0.13 | 0.13 | 1.86 |
| sized_25_with_clients | virtual | busy_%_of_a_core | 0.64 | 0.64 | 1.80 | 0.48 | 0.48 | 1.86 |
| sized_25_devices_only | virtual | busy_%_of_a_core | 0.33 | 0.33 | 1.80 | 0.33 | 0.32 | 1.86 |
| small_20_with_clients | virtual | busy_%_of_a_core | 0.12 | 0.12 | 1.80 | 0.11 | 0.11 | 1.86 |
| small_20_devices_only | virtual | busy_%_of_a_core | 0.07 | 0.07 | 1.80 | 0.07 | 0.07 | 1.86 |
| small_25_with_clients | virtual | busy_%_of_a_core | 0.15 | 0.14 | 1.80 | 0.14 | 0.14 | 1.87 |
| small_25_devices_only | virtual | busy_%_of_a_core | 0.09 | 0.09 | 1.80 | 0.09 | 0.09 | 1.87 |
| mixed_with_clients | virtual | busy_%_of_a_core | 0.18 | 0.18 | 1.80 | 0.15 | 0.15 | 1.87 |
| mixed_devices_only | virtual | busy_%_of_a_core | 0.10 | 0.10 | 1.80 | 0.10 | 0.10 | 1.87 |

- `sized_10_with_clients`: messages=1600 per 10 virtual s; x386 faster than real time; cpu 0.240040 %
- `sized_10_devices_only`: bytes=3875200 per 10 virtual s; x752 faster than real time; cpu 0.120043 %
- `sized_25_with_clients`: messages=4000 per 10 virtual s; x155 faster than real time; cpu 0.640030 %
- `sized_25_devices_only`: bytes=9688000 per 10 virtual s; x307 faster than real time; cpu 0.320046 %
- `small_20_with_clients`: messages=3200 per 10 virtual s; x822 faster than real time; cpu 0.120034 %
- `small_20_devices_only`: bytes=57456 per 10 virtual s; x1385 faster than real time; cpu 0.079937 %
- `small_25_with_clients`: messages=4000 per 10 virtual s; x657 faster than real time; cpu 0.160036 %
- `small_25_devices_only`: bytes=71964 per 10 virtual s; x1102 faster than real time; cpu 0.080047 %
- `mixed_with_clients`: messages=2850 per 10 virtual s; x546 faster than real time; cpu 0.200034 %
- `mixed_devices_only`: bytes=1493682 per 10 virtual s; x1001 faster than real time; cpu 0.101046 %

## Compile time of protocol::client<P>

- compiler gcc: g++ (GCC) 13.2.0
- compiler clang: clang version 23.1.0

| Toolchain | Mode | Variant | Median s | Min s | Peak MB | Load |
| --- | --- | --- | --- | --- | --- | --- |
| gcc | O0 | device_m1_20_methods | 7.20 | 7.17 | 771 | 2.44 |
| gcc | O0 | device_m2_15_methods | 5.72 | 5.71 | 700 | 2.53 |
| gcc | O0 | headers | 1.69 | 1.69 | 385 | 2.20 |
| gcc | O0 | table7_one_method | 3.06 | 3.04 | 500 | 2.17 |
| gcc | O2 | device_m1_20_methods | 13.15 | 13.13 | 997 | 2.93 |
| gcc | O2 | device_m2_15_methods | 10.50 | 10.49 | 802 | 2.83 |
| gcc | O2 | headers | 1.80 | 1.79 | 395 | 2.81 |
| gcc | O2 | table7_one_method | 5.06 | 5.03 | 520 | 3.40 |
| gcc | syntax | device_m1_20_methods | 2.53 | 2.53 | 528 | 2.05 |
| gcc | syntax | device_m2_15_methods | 2.08 | 2.08 | 475 | 2.04 |
| gcc | syntax | headers | 1.47 | 1.46 | 358 | 1.97 |
| gcc | syntax | table7_one_method | 1.60 | 1.60 | 389 | 2.06 |
| clang | O0 | device_m1_20_methods | 4.15 | 4.09 | 510 | 2.49 |
| clang | O0 | device_m2_15_methods | 3.50 | 3.45 | 451 | 2.64 |
| clang | O0 | headers | 1.51 | 1.51 | 270 | 2.69 |
| clang | O0 | table7_one_method | 2.13 | 2.12 | 324 | 2.58 |
| clang | O2 | device_m1_20_methods | 15.71 | 15.68 | 1201 | 2.92 |
| clang | O2 | device_m2_15_methods | 11.38 | 11.35 | 758 | 2.89 |
| clang | O2 | headers | 1.56 | 1.55 | 271 | 2.67 |
| clang | O2 | table7_one_method | 3.56 | 3.56 | 331 | 3.15 |
| clang | syntax | device_m1_20_methods | 2.44 | 2.41 | 389 | 2.72 |
| clang | syntax | device_m2_15_methods | 2.16 | 2.16 | 359 | 2.75 |
| clang | syntax | headers | 1.42 | 1.42 | 264 | 2.76 |
| clang | syntax | table7_one_method | 1.56 | 1.56 | 281 | 2.70 |

