// Compile checks of LUMEX_DEFINE_FIELD_NAMES and of the registered field
// reflection, built by cmake.field_names_compile_checks. Exactly one of
// LUMEX_FIELD_NAMES_GOOD_CASE or LUMEX_FIELD_NAMES_BAD_CASE=<n> is defined;
// the fixture compiles with warnings as errors. Every bad case is a use that
// must be refused with the message of its own check: a missing registration
// where the standard gives no automatic names or members (cases 1-3), a
// registration that does not match the aggregate (4-7, 9-12) and an index past
// the last field (8).

#include <cstddef>
#include <string>

#include "lumex/core/reflection/field_reflection/LumexAggregateFields.hpp"
#if defined(LUMEX_FIELD_NAMES_GOOD_CASE) || LUMEX_FIELD_NAMES_BAD_CASE == 3
#include "lumex/core/reflection/field_reflection/LumexFieldReflection.hpp"
#endif

using namespace lumex::core::reflection::field_reflection;

namespace check_types
{
struct plain_t
{
  int id;
  std::string name;
};

struct registered_t
{
  int id;
  std::string name;
};
LUMEX_DEFINE_FIELD_NAMES (registered_t, id, name);

struct empty_t
{
};

struct array_t
{
  int values[3];
  int tail;
};

struct closed_t
{
  int visible;

private:
  int hidden;
};

struct ref_member_t
{
  int &ref;
};

struct big_t
{
  int f0;
  int f1;
  int f2;
  int f3;
  int f4;
  int f5;
  int f6;
  int f7;
  int f8;
  int f9;
  int f10;
  int f11;
  int f12;
  int f13;
  int f14;
  int f15;
  int f16;
  int f17;
  int f18;
  int f19;
  int f20;
  int f21;
  int f22;
  int f23;
  int f24;
  int f25;
  int f26;
  int f27;
  int f28;
  int f29;
  int f30;
  int f31;
  int f32;
};
} // namespace check_types

namespace check_types
{
#if defined(LUMEX_FIELD_NAMES_GOOD_CASE)
#elif LUMEX_FIELD_NAMES_BAD_CASE == 4
// Fewer names than fields.
LUMEX_DEFINE_FIELD_NAMES (plain_t, id);
#elif LUMEX_FIELD_NAMES_BAD_CASE == 5
// More names than fields (a name twice).
LUMEX_DEFINE_FIELD_NAMES (plain_t, id, name, name);
#elif LUMEX_FIELD_NAMES_BAD_CASE == 6
// A name that is not a member.
LUMEX_DEFINE_FIELD_NAMES (plain_t, id, nope);
#elif LUMEX_FIELD_NAMES_BAD_CASE == 7
// An array member counts as several fields.
LUMEX_DEFINE_FIELD_NAMES (array_t, values, tail);
#elif LUMEX_FIELD_NAMES_BAD_CASE == 9
// More names than the macro takes (32).
LUMEX_DEFINE_FIELD_NAMES (big_t, f0, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10,
                          f11, f12, f13, f14, f15, f16, f17, f18, f19, f20,
                          f21, f22, f23, f24, f25, f26, f27, f28, f29, f30,
                          f31, f32);
#elif LUMEX_FIELD_NAMES_BAD_CASE == 10
// The same aggregate registered twice.
LUMEX_DEFINE_FIELD_NAMES (plain_t, id, name);
LUMEX_DEFINE_FIELD_NAMES (plain_t, id, name);
#elif LUMEX_FIELD_NAMES_BAD_CASE == 11
// A private member cannot be named.
LUMEX_DEFINE_FIELD_NAMES (closed_t, visible, hidden);
#elif LUMEX_FIELD_NAMES_BAD_CASE == 12
// A reference member has no pointer to member.
LUMEX_DEFINE_FIELD_NAMES (ref_member_t, ref);
#endif
} // namespace check_types

using namespace check_types;

int
main ()
{
  int used = 0;
  plain_t plain = { 1, "a" };
  registered_t registered = { 2, "b" };
  (void)plain;
  (void)registered;

#if defined(LUMEX_FIELD_NAMES_GOOD_CASE)
  // The registered uses, in every standard.
  used += static_cast<int> (names_as_array<registered_t> ().size ());
  used += names_as_array<registered_t> ()[1][0] == 'n' ? 1 : 0;
  used += get<0> (registered);
  used += static_cast<int> (get<1> (registered).size ());
  registered_t const frozen = registered;
  used += get<0> (frozen);
  used += static_cast<int> (tuple_size<registered_t>::value);
  // An aggregate without fields needs no registration.
  used += static_cast<int> (names_as_array<empty_t> ().size ());
  used += static_cast<int> (to_json (empty_t ()).size ());
  used += static_cast<int> (to_json (registered).size ());
#if __cplusplus >= 201402L
  // The automatic get of an unregistered aggregate.
  used += get<0> (plain);
#endif
#if __cplusplus >= 202002L
  // The compiler's names of an unregistered aggregate.
  used += static_cast<int> (names_as_array<plain_t> ().size ());
  used += static_cast<int> (to_json (plain).size ());
#endif
#elif LUMEX_FIELD_NAMES_BAD_CASE == 1
  // names_as_array of an unregistered aggregate below C++20.
  used += static_cast<int> (names_as_array<plain_t> ().size ());
#elif LUMEX_FIELD_NAMES_BAD_CASE == 2
  // get of an unregistered aggregate below C++14.
  used += get<0> (plain);
#elif LUMEX_FIELD_NAMES_BAD_CASE == 3
  // to_json of an unregistered aggregate below C++20.
  used += static_cast<int> (to_json (plain).size ());
#elif LUMEX_FIELD_NAMES_BAD_CASE == 8
  // An index past the last field of a registered aggregate.
  used += get<2> (registered);
#endif
  return used;
}
