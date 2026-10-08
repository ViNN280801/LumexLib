// LumexCoreDumpInstance.cxx11.tests.cpp
//
// The template overloads of core_dump_generator::generate_instance_dump (a
// string-like reason, with and without an error code) on a generator. They
// work from C++11 (before the port they were constrained by the concept
// StringLike and existed only from C++20); which argument types they take is
// checked in LumexCoreDumpConfigStrings, what they do with them here.
//
// A generator exists only after initialize (), which installs signal
// handlers, rewrites the machine-wide core pattern through sudo and starts a
// monitor thread: not for a unit test. So the tests reach the private state
// the way a test can without changing the library, by naming a member in an
// explicit instantiation (which the access rules do not check): the private
// "initialized" flag, set only while instance () creates the default
// instance, which installs nothing and is not initialized, so a dump is never
// written.
#include <atomic>
#include <string>
#if __cplusplus >= 201703L
#include <string_view>
#endif
#include <system_error>

#include <gtest/gtest.h>

#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/tests/core/utility/dump/LumexCoreDumpTestTypes.hpp"

using lumex::core::utility::dump::core_dump_generator;
using lumex_dump_test::derived_text;
using lumex_dump_test::implicit_text;

namespace
{
// --- Access to the private state ---

template <typename Tag> struct slot
{
  static typename Tag::type value;
};

template <typename Tag>
typename Tag::type slot<Tag>::value = typename Tag::type ();

/// Stores `Member` in the slot of `Tag` when the program starts.
template <typename Tag, typename Tag::type Member> struct reveal
{
  reveal () { slot<Tag>::value = Member; }

  static reveal const instance;
};

template <typename Tag, typename Tag::type Member>
reveal<Tag, Member> const reveal<Tag, Member>::instance;

struct initialized_tag
{
  using type = std::atomic_bool *;
};

template struct reveal<initialized_tag, &core_dump_generator::s_initialized>;

/// The default instance, created without initialize (): instance () refuses
/// while the flag is false, so the flag is true for the call only.
core_dump_generator &
default_instance ()
{
  std::atomic_bool &initialized = *slot<initialized_tag>::value;
  bool const before = initialized.exchange (true);
  try
    {
      core_dump_generator &instance = core_dump_generator::instance ();
      initialized.store (before);
      return instance;
    }
  catch (...)
    {
      initialized.store (before);
      throw;
    }
}
} // namespace

// --- generate_instance_dump with the template overloads ---

// The default instance is not initialized, so a dump is never written: the
// call reports false (and invalid_argument with an error code). That makes the
// forwarding of every accepted argument type safe to run.

TEST (
    LumexCoreDumpInstanceGenerateTest,
    GivenUninitializedInstance_WhenGenerateInstanceDumpWithEveryType_ThenFalse)
{
  core_dump_generator &generator = default_instance ();
  ASSERT_FALSE (generator.is_instance_initialized ());
  char const *const pointer = "reason";
  EXPECT_FALSE (generator.generate_instance_dump (std::string ("reason")));
  EXPECT_FALSE (generator.generate_instance_dump (pointer));
  EXPECT_FALSE (generator.generate_instance_dump ("reason"));
  EXPECT_FALSE (generator.generate_instance_dump (implicit_text ("reason")));
  EXPECT_FALSE (generator.generate_instance_dump (derived_text ("reason")));
  EXPECT_FALSE (generator.generate_instance_dump ());
}

TEST (
    LumexCoreDumpInstanceGenerateTest,
    GivenUninitializedInstance_WhenGenerateInstanceDumpWithErrorCode_ThenFalseAndInvalidArgument)
{
  core_dump_generator &generator = default_instance ();
  char const *const pointer = "reason";
  std::error_code code;

  EXPECT_FALSE (
      generator.generate_instance_dump (std::string ("reason"), code));
  EXPECT_TRUE (code == std::errc::invalid_argument);

  code.clear ();
  EXPECT_FALSE (generator.generate_instance_dump (pointer, code));
  EXPECT_TRUE (code == std::errc::invalid_argument);

  code.clear ();
  EXPECT_FALSE (generator.generate_instance_dump ("reason", code));
  EXPECT_TRUE (code == std::errc::invalid_argument);

  code.clear ();
  EXPECT_FALSE (
      generator.generate_instance_dump (implicit_text ("reason"), code));
  EXPECT_TRUE (code == std::errc::invalid_argument);
}

#if __cplusplus >= 201703L

TEST (
    LumexCoreDumpInstanceGenerateTest,
    GivenUninitializedInstance_WhenGenerateInstanceDumpWithStringView_ThenFalse)
{
  core_dump_generator &generator = default_instance ();
  std::string_view const reason = "reason";
  std::error_code code;
  EXPECT_FALSE (generator.generate_instance_dump (reason));
  EXPECT_FALSE (generator.generate_instance_dump (reason, code));
  EXPECT_TRUE (code == std::errc::invalid_argument);
}

#endif
