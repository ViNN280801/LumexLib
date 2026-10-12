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

// The aggregates of the tests of the recursion of to_json into nested
// aggregates. None has a to_json of its own except where the name says so
// (NestHooked, NestNoNames): without one, to_json itself writes a nested
// aggregate as a JSON object. The registered ones are small on purpose; every
// instantiation per member costs compile time.

#ifndef LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_NESTED_HPP
#define LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_NESTED_HPP

#include <array>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/reflection/LumexReflection"

namespace lumex_field_reflection_tests
{
struct NestPoint
{
  int x;
  int y;
};
LUMEX_DEFINE_FIELD_NAMES (NestPoint, x, y);

// An aggregate in an aggregate in an aggregate.
struct NestSegment
{
  NestPoint from;
  NestPoint to;
};
LUMEX_DEFINE_FIELD_NAMES (NestSegment, from, to);

struct NestPath
{
  std::string name;
  NestSegment first;
  int length;
};
LUMEX_DEFINE_FIELD_NAMES (NestPath, name, first, length);

// Containers of aggregates: a vector, a std::array, a map with string keys.
struct NestCollections
{
  std::vector<NestPoint> list;
  std::array<NestPoint, 2> pair;
  std::map<std::string, NestPoint> named;
};
LUMEX_DEFINE_FIELD_NAMES (NestCollections, list, pair, named);

// A container of containers of aggregates.
struct NestMatrix
{
  std::vector<std::vector<NestPoint>> rows;
};
LUMEX_DEFINE_FIELD_NAMES (NestMatrix, rows);

// Optional-like fields that hold aggregates, and a container of optionals.
struct NestOptionals
{
  optional<NestPoint> anchor;
  optional<std::vector<NestPoint>> trail;
  std::vector<optional<NestPoint>> slots;
  int id;
};
LUMEX_DEFINE_FIELD_NAMES (NestOptionals, anchor, trail, slots, id);

// A nested aggregate without fields.
struct NestVoid
{
};

struct NestWithVoid
{
  int value;
  NestVoid nothing;
};
LUMEX_DEFINE_FIELD_NAMES (NestWithVoid, value, nothing);

// A nested aggregate that has a to_json of its own, which writes a number:
// the hook has priority over the recursion, although the names are known.
struct NestHooked
{
  int a;
  int b;
};
LUMEX_DEFINE_FIELD_NAMES (NestHooked, a, b);

inline void
to_json (nlohmann::json &j, NestHooked const &value)
{
  j = value.a + value.b;
}

struct NestUsesHooked
{
  NestHooked single;
  std::vector<NestHooked> list;
};
LUMEX_DEFINE_FIELD_NAMES (NestUsesHooked, single, list);

// A nested aggregate whose names nobody registered, saved by a to_json of its
// own: it is never asked for its names, so it works below C++20 too.
struct NestNoNames
{
  int p;
  int q;
};

inline void
to_json (nlohmann::json &j, NestNoNames const &value)
{
  j = nlohmann::json::array ();
  j.push_back (value.p);
  j.push_back (value.q);
}

struct NestUsesNoNames
{
  NestNoNames pair;
};
LUMEX_DEFINE_FIELD_NAMES (NestUsesNoNames, pair);
} // namespace lumex_field_reflection_tests

#endif // !LUMEX_TESTS_CORE_REFLECTION_FIELD_REFLECTION_NESTED_HPP
