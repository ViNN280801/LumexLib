/*
 * Copyright (c) 2026 Vladislav Semykin <vladislav.semykin@gmail.com>
 *
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of
 * charge, to any person obtaining a copy
 * of this software and associated
 * documentation files (the "Software"), to deal
 * in the Software without
 * restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef LUMEX_TESTS_APPLIED_JSON_HPP
#define LUMEX_TESTS_APPLIED_JSON_HPP

#include <string>

#include <nlohmann/json.hpp>

// The schema and the documents the tests of the schema (schema/), the
// validator (validation/) and the normalizer (normalization/) walk. The
// functions are inline in a named namespace, so every test source of the three
// directories has the same definitions.

namespace lumex_json_schema_test
{
inline nlohmann::json
person_schema ()
{
  return nlohmann::json::parse (R"({
    "type": "object",
    "required": ["name", "age", "address", "tags"],
    "properties": {
      "name":    { "type": "string" },
      "age":     { "type": "integer" },
      "nick":    { "type": ["string", "null"] },
      "role":    { "type": "string", "enum": ["admin", "user"] },
      "kind":    { "const": "person" },
      "address": {
        "type": "object",
        "required": ["city"],
        "properties": { "city": { "type": "string" }, "zip": { "type": "string" } }
      },
      "tags":    { "type": "array", "items": { "type": "string" } },
      "extra":   { "type": ["object", "null"], "properties": {} },
      "any":     { "type": "array" }
    }
  })");
}

inline nlohmann::json
valid_person ()
{
  return nlohmann::json::parse (R"({
    "name": "Ann", "age": 30, "role": "admin", "kind": "person",
    "address": { "city": "Oslo", "zip": "0150", "unknown": 1 },
    "tags": ["a", "b"], "ignored": true
  })");
}

inline std::string
text (nlohmann::json const &document)
{
  return document.dump ();
}
} // namespace lumex_json_schema_test

#endif // !LUMEX_TESTS_APPLIED_JSON_HPP
