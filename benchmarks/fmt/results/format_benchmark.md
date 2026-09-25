# LumexFormat benchmark results

Environment: `compiler=MSVC 1951 cplusplus=202002 build=release`.

Median ns per call over the repetitions (lower is better); `x LumexFormat` is the time relative to LumexFormat (above 1.0 = slower than LumexFormat).

| Scenario | Method | Median, ns | Min, ns | Max, ns | x LumexFormat |
| --- | --- | ---: | ---: | ---: | ---: |
| `int` | LumexFormat | 84.8 | 81.1 | 109.7 | 1.00 |
| `int` | std::format | 62.6 | 59.4 | 74.0 | 0.74 |
| `int` | ostringstream | 473.0 | 443.5 | 501.7 | 5.58 |
| `int` | snprintf | 52.4 | 48.6 | 64.0 | 0.62 |
| `int` | to_string | 12.5 | 11.0 | 18.3 | 0.15 |
| `int_hex_padded` | LumexFormat | 83.0 | 80.4 | 93.8 | 1.00 |
| `int_hex_padded` | std::format | 116.2 | 113.3 | 128.4 | 1.40 |
| `int_hex_padded` | ostringstream | 505.6 | 499.8 | 531.8 | 6.09 |
| `int_hex_padded` | snprintf | 78.8 | 74.3 | 80.8 | 0.95 |
| `double_fixed` | LumexFormat | 143.1 | 139.7 | 154.0 | 1.00 |
| `double_fixed` | std::format | 117.7 | 114.3 | 238.5 | 0.82 |
| `double_fixed` | ostringstream | 720.5 | 694.0 | 856.9 | 5.03 |
| `double_fixed` | snprintf | 200.2 | 189.4 | 211.5 | 1.40 |
| `double_shortest` | LumexFormat | 169.3 | 159.4 | 180.8 | 1.00 |
| `double_shortest` | std::format | 111.0 | 106.8 | 115.8 | 0.66 |
| `double_shortest` | to_string | 361.9 | 347.0 | 372.3 | 2.14 |
| `string_padded` | LumexFormat | 154.8 | 149.1 | 168.8 | 1.00 |
| `string_padded` | std::format | 469.9 | 455.2 | 492.2 | 3.04 |
| `string_padded` | ostringstream | 382.0 | 372.7 | 401.6 | 2.47 |
| `string_padded` | snprintf | 100.2 | 95.3 | 108.3 | 0.65 |
| `log_line` | LumexFormat | 405.7 | 394.2 | 416.8 | 1.00 |
| `log_line` | std::format | 321.9 | 316.9 | 342.0 | 0.79 |
| `log_line` | ostringstream | 1201.0 | 1178.1 | 1242.3 | 2.96 |
| `log_line` | snprintf | 445.7 | 430.7 | 471.9 | 1.10 |
| `ten_args` | LumexFormat | 525.3 | 506.8 | 562.9 | 1.00 |
| `ten_args` | std::format | 357.5 | 346.3 | 369.6 | 0.68 |
| `ten_args` | ostringstream | 1885.7 | 1850.0 | 1911.6 | 3.59 |
| `ten_args` | snprintf | 321.3 | 304.5 | 344.5 | 0.61 |
| `append_to_buffer` | LumexFormat | 222.8 | 214.8 | 241.8 | 1.00 |
| `append_to_buffer` | std::format | 169.8 | 162.6 | 177.6 | 0.76 |
| `append_to_buffer` | ostringstream | 807.9 | 774.9 | 825.0 | 3.63 |
| `append_to_buffer` | snprintf | 245.8 | 238.0 | 263.3 | 1.10 |

Scenarios:

- `int`: "{}" of an int
- `int_hex_padded`: "{:#010x}" of an int
- `double_fixed`: "{:.3f}" of a double
- `double_shortest`: "{}" of a double (shortest round trip; to_string prints 6 fixed digits, shown for scale)
- `string_padded`: "{:>16}" of a std::string
- `log_line`: "channel {:>3} flow {:8.3f} ml/min state {}"
- `ten_args`: ten "{}" fields (ints and strings)
- `append_to_buffer`: format_to into a reused std::string
