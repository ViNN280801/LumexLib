#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Checks that lumex/examples/fmt shows every public feature of lumex::fmt.

Every public function and type, every part of the format specification,
every presentation type, the range / tuple options and every chrono
conversion must appear in at least one example. Run from LumexLib/:

    python Scripts/CodeTools/check_fmt_examples_coverage.py

Exits with 1 and lists what is missing. Extend the tables below when the
public API of lumex/core/fmt grows.
"""

import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
EXAMPLES = os.path.join(ROOT, "lumex", "examples", "fmt", "*.cpp")

# Public functions and types: a plain substring must appear.
API = [
    "fmt::format (", "fmt::format (L", "fmt::format (grouped",
    "fmt::format_to (",
    "fmt::format_to_n (", "fmt::formatted_size (", "fmt::vformat (",
    "fmt::vformat_to (", "fmt::make_format_args (", "fmt::make_wformat_args (",
    "fmt::arg (", "fmt::runtime (", "fmt::try_format (", "fmt::print (",
    "fmt::println (", "fmt::streamed (", "fmt::FormatError", ".position ()",
    "FormatError::no_position ()", "Formatter<", "OstreamFormatter<",
    "BasicAppender<", "FormatContext", "WFormatContext", "FormatParseContext",
    "WFormatParseContext", "BasicFormatContext<", "BasicFormatParseContext<",
    "fmt::FormatArgs", "fmt::WFormatArgs", "fmt::FormatString<",
    "fmt::WFormatString<", "fmt::format_to_n_result_t<",
    "fmt::try_format_result_t<", ".set_separator (", ".set_brackets (",
    ".underlying ()", "LumexStringView", "std::string_view", "std::locale",
]

# Format-string features: a regular expression over the example sources.
SPEC = {
    "automatic field {}": r'"[^"]*\{\}',
    "manual field {0}": r'\{\d\}',
    "named field {name}": r'\{[a-z_]+\}',
    "escaped braces {{ }}": r'\{\{',
    "fill + align <": r'\{:[^{}]?<\d',
    "fill + align >": r'\{:[^{}]>\d',
    "fill + align ^": r'\{:[^{}]\^\d',
    "multi-byte fill": r'\{:\\x[0-9A-F]{2}\\x[0-9A-F]{2}\^',
    "sign +": r'\{:[^}]*\+',
    "sign -": r'\{:-\}',
    "sign space": r'\{: \}',
    "alternate #": r'\{:[^}]*#',
    "zero padding 0": r'\{:[+#]?0\d',
    "dynamic width {}": r'\{:[^}]*\{\}',
    "dynamic width by index": r'\{\d:\{\d\}',
    "dynamic width by name": r'\{:\{[a-z]+\}',
    "precision": r'\{:[^}]*\.\d',
    "dynamic precision": r'\.\{[^}]*\}',
    "L (locale form)": r'\{:[^}]*L',
}
for letter in "bBcdoxXeEfFgGaAspP":
    SPEC["type " + letter] = r'\{\d?:[^{}]*' + letter + r'\}'
SPEC["type ? (debug)"] = r'\{:[^}]*\?\}'
SPEC["range n"] = r'\{:n\}'
SPEC["range m"] = r'\{:m\}'
SPEC["range s"] = r'\{:s\}'
SPEC["range ?s"] = r'\{:\?s\}'
SPEC["range element spec"] = r'\{::[^}]+\}'
SPEC["nested element spec"] = r'\{:::'
for conversion in "HMSTRIprjQqntYCymdeFDaAbBhuwUWVGgcxXZz%":
    SPEC["chrono %" + conversion] = r'%' + re.escape(conversion)
SPEC["chrono E modifier"] = r'%E[cCxXyYz]'
SPEC["chrono O modifier"] = r'%O[deHImMSuUVwWyz]'
SPEC["chrono L"] = r'\{:L%'


def main():
    text = ""
    for path in sorted(glob.glob(EXAMPLES)):
        with open(path, encoding="utf-8") as stream:
            text += stream.read()
    missing = [item for item in API if item not in text]
    missing += [name for name, pattern in SPEC.items()
                if re.search(pattern, text) is None]
    total = len(API) + len(SPEC)
    if missing:
        print("lumex/examples/fmt misses %d of %d features:"
              % (len(missing), total))
        for item in missing:
            print("  -", item)
        return 1
    print("lumex/examples/fmt covers all %d features" % total)
    return 0


if __name__ == "__main__":
    sys.exit(main())
