#!/usr/bin/env python3
"""Generates LumexUtf8OracleData.hpp, the corpus of the UTF-8 decoder tests.

The oracle is CPython's UTF-8 decoder (the "maximal subpart" practice of the
Unicode standard, section 3.9, Table 3-7): the expected code points of a byte
string are what `bytes.decode ('utf-8', 'replace')` gives with the U+FFFD that
it writes for each ill-formed subsequence left out. A custom error handler that
returns the empty string does the removal, so a U+FFFD encoded in the input (EF
BF BD) is kept; the script also checks that the result equals the 'replace'
output with every U+FFFD removed for every string without EF BF BD.

The data in the header comes from CPython 3.12.0 and does not change with the
interpreter (the decoder is specified by the Unicode standard, section 3.9).
Run it again only to extend the corpus (the header it writes is already
wrapped as clang-format leaves it):

  python LumexUtf8OracleData.py [output header]

Without an argument it writes LumexUtf8OracleData.hpp next to this script.
"""
import codecs
import os
import random
import sys
import textwrap

SEED = 20261114
RANDOM_ROWS = 700


def skip_handler(err):
    return ('', err.end)


codecs.register_error('lumex_skip', skip_handler)


def expected(data):
    text = data.decode('utf-8', 'lumex_skip')
    if b'\xef\xbf\xbd' not in data:
        replaced = data.decode('utf-8', 'replace').replace('\ufffd', '')
        assert replaced == text, data.hex()
    return [ord(c) for c in text]


def valid(cp):
    if cp < 0x80:
        return bytes([cp])
    if cp < 0x800:
        return bytes([0xC0 | cp >> 6, 0x80 | cp & 0x3F])
    if cp < 0x10000:
        return bytes([0xE0 | cp >> 12, 0x80 | cp >> 6 & 0x3F, 0x80 | cp & 0x3F])
    return bytes([0xF0 | cp >> 18, 0x80 | cp >> 12 & 0x3F,
                  0x80 | cp >> 6 & 0x3F, 0x80 | cp & 0x3F])


# The bytes at the edges of the ranges of Table 3-7 and next to them.
EDGE = [0x00, 0x41, 0x7F, 0x80, 0x8F, 0x90, 0x9F, 0xA0, 0xBF, 0xC0, 0xC1,
        0xC2, 0xDF, 0xE0, 0xE1, 0xEC, 0xED, 0xEE, 0xEF, 0xF0, 0xF1, 0xF3,
        0xF4, 0xF5, 0xF7, 0xF8, 0xFF]

HAND = [
    # valid, the first and the last of every range
    'C2 80', 'DF BF', 'E0 A0 80', 'E0 BF BF', 'E1 80 80', 'EC BF BF',
    'ED 80 80', 'ED 9F BF', 'EE 80 80', 'EF BF BF', 'F0 90 80 80',
    'F0 BF BF BF', 'F1 80 80 80', 'F3 BF BF BF', 'F4 80 80 80',
    'F4 8F BF BF',
    # overlong forms
    'C0 80', 'C0 AF', 'C1 BF', 'C1 80', 'E0 80 80', 'E0 9F BF', 'E0 80 AF',
    'F0 80 80 80', 'F0 8F BF BF', 'F0 80 80 AF',
    # surrogates
    'ED A0 80', 'ED AF BF', 'ED B0 80', 'ED BF BF',
    # above U+10FFFF
    'F4 90 80 80', 'F4 BF BF BF', 'F5 80 80 80', 'F7 BF BF BF',
    'F8 88 80 80 80', 'FC 84 80 80 80 80', 'FE', 'FF',
    # truncated
    'C2', 'E0', 'E0 A0', 'E1 80', 'ED 9F', 'F0', 'F0 90', 'F0 90 80',
    'F4', 'F4 8F', 'F4 8F BF',
    # truncated in the middle, a valid character follows
    'E2 82 41', 'E2 82 C3 A9', 'F0 9F 98 41', 'F0 9F C3 A9', 'F0 41',
    'C3 41', 'E1 80 E1 80 80', 'F1 80 80 F1 80 80 80',
    # ill-formed prefix then valid
    'ED A0 80 41', 'ED A0 C3 A9', 'F4 90 80 80 41', 'C0 80 41',
    'E0 80 80 C3 A9', 'F0 80 80 80 F0 9F 98 80',
    # stray continuation
    '80', 'BF', '41 80 42', 'C3 A9 A9', 'E2 82 AC 82', '80 80 80',
    # the genuine U+FFFD and the noncharacters stay
    'EF BF BD', 'EF BF BE', 'EF BF BF', 'F0 9F BF BF', 'F4 8F BF BE',
    'EF B7 90', 'EF B7 AF',
    # NUL and embedded NUL
    '00', '41 00 42', 'C2 80 00',
]


def main():
    out_path = (sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.path.dirname(os.path.abspath(__file__)), 'LumexUtf8OracleData.hpp'))
    rng = random.Random(SEED)
    rows = []
    seen = set()

    def add(data):
        if data in seen:
            return
        seen.add(data)
        rows.append(data)

    for h in HAND:
        add(bytes.fromhex(h))

    # every single byte
    for b in range(256):
        add(bytes([b]))

    # random strings: valid characters and edge bytes, glued
    target = len(rows) + RANDOM_ROWS
    while len(rows) < target:
        n = rng.randint(1, 10)
        data = b''
        for _ in range(n):
            roll = rng.random()
            if roll < 0.35:
                data += bytes([rng.choice(EDGE)])
            elif roll < 0.55:
                data += bytes([rng.randrange(256)])
            else:
                cp = rng.choice([rng.randrange(0x80), rng.randrange(0x80, 0x800),
                                 rng.randrange(0x800, 0xD800),
                                 rng.randrange(0xE000, 0x10000),
                                 rng.randrange(0x10000, 0x110000)])
                v = valid(cp)
                if rng.random() < 0.3 and len(v) > 1:
                    v = v[:rng.randrange(1, len(v))]  # cut it
                data += v
        add(data)

    lines = []
    lines.append('// lumex/tests/core/unicode/utf/LumexUtf8OracleData.hpp')
    lines.append('//')
    # wrapped the way clang-format leaves a comment, so that a regenerated
    # header is byte-identical to the checked-in one
    note = ("Generated by LumexUtf8OracleData.py in this directory (seed %d); "
            "do not edit by hand. Every row is a UTF-8 byte string and the "
            "code points that CPython's bytes.decode ('utf-8', 'replace') "
            "gives for it with the U+FFFD that it writes for each ill-formed "
            "subsequence left out (a U+FFFD encoded in the input is kept): "
            "the \"maximal subpart\" practice of the Unicode standard "
            "(section 3.9, Table 3-7), which is what a decoder that skips "
            "ill-formed input must yield." % SEED)
    for text in textwrap.wrap(note, 76):
        lines.append('// ' + text)
    lines.append('#ifndef LUMEX_TESTS_CORE_UNICODE_UTF_HPP')
    lines.append('#define LUMEX_TESTS_CORE_UNICODE_UTF_HPP')
    lines.append('')
    lines.append('#include <cstddef>')
    lines.append('')
    lines.append('namespace unicode_test')
    lines.append('{')
    lines.append('// The input bytes and the expected code points, both as hex text '
                 'separated by')
    lines.append('// spaces (an empty text for nothing).')
    lines.append('struct oracle_row_t')
    lines.append('{')
    lines.append('  char const *bytes;')
    lines.append('  char const *points;')
    lines.append('};')
    lines.append('')
    lines.append('// clang-format off')
    lines.append('oracle_row_t const oracle_rows[] = {')
    for data in rows:
        cps = expected(data)
        lines.append('  { "%s", "%s" },' % (data.hex(' ').upper(),
                                            ' '.join('%X' % c for c in cps)))
    lines.append('};')
    lines.append('// clang-format on')
    lines.append('')
    lines.append('std::size_t const oracle_row_count')
    lines.append('    = sizeof (oracle_rows) / sizeof (oracle_rows[0]);')
    lines.append('} // namespace unicode_test')
    lines.append('')
    lines.append('#endif // !LUMEX_TESTS_CORE_UNICODE_UTF_HPP')
    with open(out_path, 'w', encoding='utf-8', newline='') as f:
        f.write('\r\n'.join(lines) + '\r\n')
    print(len(rows), 'rows')


if __name__ == '__main__':
    main()
