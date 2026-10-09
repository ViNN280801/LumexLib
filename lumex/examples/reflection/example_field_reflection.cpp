// Field reflection of simple aggregates: the number of fields, the names, the
// I-th field and a JSON dump, from C++11. The CMake target defines
// LUMEX_WITH_FIELD_REFLECTION and links nlohmann/json, which `to_json` needs
// (the umbrella includes field reflection only with that macro). The same
// source is built twice: at C++11, where the names come from a registration
// (`LUMEX_DEFINE_FIELD_NAMES`), and at C++20, where the compiler also supplies
// the names of an aggregate that nobody registered. to_json is called with
// its namespace here because this file has to_json overloads of its own, which
// hide the one of field_reflection from an unqualified call.

#include <cstddef>
#include <iostream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lumex/core/optional/LumexOptional"
#include "lumex/core/reflection/LumexReflection"

using namespace lumex::core::reflection::field_reflection;

// A simple aggregate: public members, no constructors, no base classes. The
// registration lists the members once, in declaration order, after the type
// and before the first use of field reflection on it.
struct ExamplePoint
{
  int x;
  int y;
};
LUMEX_DEFINE_FIELD_NAMES (ExamplePoint, x, y);

// A type with a to_json of its own keeps it: the overload has priority over
// the automatic writing of an aggregate, wherever the type is a field.
struct ExampleVolume
{
  int microliters;
};
LUMEX_DEFINE_FIELD_NAMES (ExampleVolume, microliters);

inline void
to_json (nlohmann::json &j, ExampleVolume const &volume)
{
  j = std::to_string (volume.microliters) + " uL";
}

// Aggregates nest: a field that is an aggregate becomes a JSON object, a
// vector of aggregates an array of objects, an empty optional is left out.
struct ExampleRun
{
  std::string name;
  ExamplePoint origin;
  std::vector<ExamplePoint> trail;
  ExampleVolume volume;
  optional<std::string> note;
};
LUMEX_DEFINE_FIELD_NAMES (ExampleRun, name, origin, trail, volume, note);

#if __cplusplus >= 202002L
// Not registered: from C++20 the compiler supplies the names.
struct ExampleUnregistered
{
  int channel;
  double flow_rate;
};
#endif

int
main ()
{
  std::cout << "=== Field reflection: names, get and to_json ===\n\n";

  std::cout << "--- 1. Count and names of a registered aggregate ---\n";
  std::cout << "fields=" << tuple_size<ExamplePoint>::value << " names:";
  for (char const *name : names_as_array<ExamplePoint> ())
    std::cout << ' ' << name;
  std::cout << '\n';

  std::cout << "\n--- 2. get<I> reads and writes a field ---\n";
  ExamplePoint point = { 3, 4 };
  get<0> (point) = 30;
  std::cout << "x=" << get<0> (point) << " y=" << get<1> (point) << '\n';

  std::cout << "\n--- 3. to_json of nested aggregates ---\n";
  ExampleRun run;
  run.name = "run-1";
  run.origin.x = 1;
  run.origin.y = 2;
  run.trail.push_back (point);
  run.trail.push_back (run.origin);
  run.volume.microliters = 25;
  nlohmann::json const document
      = lumex::core::reflection::field_reflection::to_json (run);
  std::cout << document.dump (2) << '\n';

  std::cout << "\n--- 4. An engaged optional is written ---\n";
  run.note = std::string ("checked");
  std::cout << lumex::core::reflection::field_reflection::to_json (run)["note"]
            << '\n';

#if __cplusplus >= 202002L
  std::cout << "\n--- 5. C++20: the compiler's names, no registration ---\n";
  for (char const *name : names_as_array<ExampleUnregistered> ())
    std::cout << ' ' << name;
  ExampleUnregistered const unregistered = { 2, 0.5 };
  std::cout << '\n'
            << lumex::core::reflection::field_reflection::to_json (
                   unregistered)
                   .dump ()
            << '\n';
#endif

  // The example doubles as a test: it fails when the document is not the
  // expected one.
  std::string const expected
      = "{\"name\":\"run-1\",\"origin\":{\"x\":1,\"y\":2},"
        "\"trail\":[{\"x\":30,\"y\":4},{\"x\":1,\"y\":2}],"
        "\"volume\":\"25 uL\"}";
  if (document.dump () != expected)
    {
      std::cout << "unexpected document: " << document.dump () << '\n';
      return 1;
    }

  std::cout << "\n=== Field reflection example finished ===\n";
  return 0;
}
