// LumexCoreDumpInstance.cxx11.tests.cpp
//
// The members of core_dump_generator that depend on the standard and that need
// state: get_memory_filters_range (static; the view of the filters of the
// current configuration), get_optional_dump_directory and
// get_dump_directory_if_set (the directory of an instance), and the template
// overloads of generate_instance_dump. They work from C++11: the range is the
// iterator_range of this library before C++20 and std::ranges::ref_view from
// it, the optional is the optional of this library before C++17 and
// std::optional from it (the alias types are checked in LumexCoreDumpInstance
// .cxx17 and .cxx20; everything the two forms share is run here, in every
// suite).
//
// There is no public way to put a filter into the static configuration or a
// directory into an instance except initialize (), which installs signal
// handlers, rewrites the machine-wide core pattern through sudo and starts a
// monitor thread: not for a unit test. So the tests reach the private state
// the way a test can without changing the library, by naming the members in an
// explicit instantiation (which the access rules do not check): the static
// configuration, the private "initialized" flag (set only while instance ()
// creates the default instance, which installs nothing) and the directory of
// that instance. The fixture restores all three before and after each test.
#include <atomic>
#include <cstddef>
#include <iterator>
#include <string>
#if __cplusplus >= 201703L
#include <string_view>
#endif
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#if __cplusplus < 201703L
#include "lumex/core/optional/opt/LumexOptional.hpp"
#endif
#include "lumex/core/utility/compiler/LumexCheckFeatures.hpp"
#include "lumex/core/utility/dump/LumexCoreDumpGenerator.hpp"
#include "lumex/core/utility/ranges/LumexIteratorRange.hpp"
#include "lumex/tests/core/utility/dump/LumexCoreDumpTestTypes.hpp"

using lumex::core::utility::dump::core_dump_generator;
using lumex::core::utility::dump::dump_configuration;
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

struct configuration_tag
{
  using type = dump_configuration *;
};

struct initialized_tag
{
  using type = std::atomic_bool *;
};

struct directory_tag
{
  using type = std::string core_dump_generator::*;
};

template struct reveal<configuration_tag,
                       &core_dump_generator::s_currentConfig>;
template struct reveal<initialized_tag, &core_dump_generator::s_initialized>;
template struct reveal<directory_tag, &core_dump_generator::m_dumpDirectory>;

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

dump_configuration &
current_configuration ()
{
  return *slot<configuration_tag>::value;
}

void
set_instance_directory (std::string const &directory)
{
  default_instance ().*slot<directory_tag>::value = directory;
}

class LumexCoreDumpInstanceTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    reset_state ();
  }

  void
  TearDown () override
  {
    reset_state ();
  }

  static void
  reset_state ()
  {
    current_configuration () = dump_configuration ();
    set_instance_directory (std::string ());
  }
};

using memory_filters_range_t = core_dump_generator::memory_filters_range_t;
using optional_dump_directory_t
    = core_dump_generator::optional_dump_directory_t;

std::vector<std::string>
walk (memory_filters_range_t const &range)
{
  std::vector<std::string> walked;
  for (std::string const &filter : range)
    walked.push_back (filter);
  return walked;
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

// --- get_memory_filters_range ---

TEST_F (LumexCoreDumpInstanceTest,
        GivenNoFilters_WhenMemoryFiltersRange_ThenEmpty)
{
  memory_filters_range_t const range
      = core_dump_generator::get_memory_filters_range ();
  EXPECT_TRUE (range.begin () == range.end ());
  EXPECT_TRUE (range.empty ());
  EXPECT_EQ (std::distance (range.begin (), range.end ()), 0);
  EXPECT_TRUE (walk (range).empty ());
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenFilters_WhenMemoryFiltersRange_ThenWalksThemInOrder)
{
  ASSERT_TRUE (current_configuration ().add_memory_filter ("heap"));
  ASSERT_TRUE (
      current_configuration ().add_memory_filter (std::string ("stack")));
  ASSERT_TRUE (
      current_configuration ().add_memory_filter (implicit_text ("anon")));
  memory_filters_range_t const range
      = core_dump_generator::get_memory_filters_range ();
  EXPECT_FALSE (range.empty ());
  EXPECT_EQ (std::distance (range.begin (), range.end ()), 3);
  std::vector<std::string> const expected = { "heap", "stack", "anon" };
  EXPECT_EQ (walk (range), expected);
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenFilters_WhenMemoryFiltersRange_ThenViewsTheListWithoutCopying)
{
  ASSERT_TRUE (current_configuration ().add_memory_filter ("heap"));
  memory_filters_range_t const range
      = core_dump_generator::get_memory_filters_range ();
  ASSERT_FALSE (range.empty ());
  EXPECT_EQ (&*range.begin (),
             &current_configuration ().get_memory_filters ().front ());
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenFilterAddedLater_WhenNewMemoryFiltersRange_ThenSeesIt)
{
  EXPECT_TRUE (core_dump_generator::get_memory_filters_range ().empty ());
  ASSERT_TRUE (current_configuration ().add_memory_filter ("heap"));
  EXPECT_FALSE (core_dump_generator::get_memory_filters_range ().empty ());
  current_configuration ().clear_memory_filters ();
  EXPECT_TRUE (core_dump_generator::get_memory_filters_range ().empty ());
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenCopiedRange_WhenWalked_ThenBothSeeTheSameFilters)
{
  ASSERT_TRUE (current_configuration ().add_memory_filter ("heap"));
  ASSERT_TRUE (current_configuration ().add_memory_filter ("stack"));
  memory_filters_range_t const first
      = core_dump_generator::get_memory_filters_range ();
  memory_filters_range_t const copy = first;
  EXPECT_EQ (walk (first), walk (copy));
  EXPECT_TRUE (first.begin () == copy.begin ());
  EXPECT_TRUE (first.end () == copy.end ());
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenFilters_WhenWalkedWithIterators_ThenUsable)
{
  ASSERT_TRUE (current_configuration ().add_memory_filter ("heap"));
  ASSERT_TRUE (current_configuration ().add_memory_filter ("stack"));
  memory_filters_range_t const range
      = core_dump_generator::get_memory_filters_range ();
  auto iterator = range.begin ();
  EXPECT_EQ (*iterator, "heap");
  ++iterator;
  EXPECT_EQ (*iterator, "stack");
  ++iterator;
  EXPECT_TRUE (iterator == range.end ());
  static_assert (
      std::is_same<decltype (*range.begin ()), std::string const &>::value,
      "the elements are constant strings");
}

TEST (LumexCoreDumpInstanceTypeTest,
      GivenMemoryFiltersRange_WhenInspected_ThenNoexceptAndTheAlias)
{
  static_assert (noexcept (core_dump_generator::get_memory_filters_range ()),
                 "get_memory_filters_range is noexcept");
  static_assert (
      std::is_same<decltype (core_dump_generator::get_memory_filters_range ()),
                   memory_filters_range_t>::value,
      "the function returns the alias");
  SUCCEED ();
}

#if !LUMEX_HAS_STD_RANGES

TEST (LumexCoreDumpInstanceTypeTest,
      GivenNoStdRanges_WhenMemoryFiltersRange_ThenTheIteratorRangeOfTheLibrary)
{
  static_assert (
      std::is_same<memory_filters_range_t,
                   lumex::core::utility::ranges::iterator_range<
                       std::vector<std::string>::const_iterator>>::value,
      "iterator_range over the constant iterators of the filter list");
  static_assert (std::is_same<memory_filters_range_t::iterator,
                              std::vector<std::string>::const_iterator>::value,
                 "the iterator is the constant iterator");
  SUCCEED ();
}

#endif

#if LUMEX_HAS_STD_RANGES

TEST_F (LumexCoreDumpInstanceTest,
        GivenStdRanges_WhenMemoryFiltersRange_ThenSizeIsTheCountOfFilters)
{
  EXPECT_EQ (core_dump_generator::get_memory_filters_range ().size (), 0u);
  ASSERT_TRUE (current_configuration ().add_memory_filter ("heap"));
  ASSERT_TRUE (current_configuration ().add_memory_filter ("stack"));
  EXPECT_EQ (core_dump_generator::get_memory_filters_range ().size (), 2u);
  EXPECT_EQ (core_dump_generator::get_memory_filters_range ().front (),
             "heap");
  EXPECT_EQ (core_dump_generator::get_memory_filters_range ().back (),
             "stack");
}

#endif

// --- get_optional_dump_directory and get_dump_directory_if_set ---

TEST_F (LumexCoreDumpInstanceTest,
        GivenNoDirectory_WhenGetOptionalDumpDirectory_ThenEmpty)
{
  core_dump_generator const &generator = default_instance ();
  optional_dump_directory_t const directory
      = generator.get_optional_dump_directory ();
  EXPECT_FALSE (directory.has_value ());
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenDirectory_WhenGetOptionalDumpDirectory_ThenHoldsIt)
{
  set_instance_directory ("/var/lib/app/dumps");
  core_dump_generator const &generator = default_instance ();
  optional_dump_directory_t const directory
      = generator.get_optional_dump_directory ();
  ASSERT_TRUE (directory.has_value ());
  EXPECT_EQ (*directory, "/var/lib/app/dumps");
  EXPECT_EQ (directory.value (), "/var/lib/app/dumps");
  EXPECT_EQ (directory.value_or (std::string ("fallback")),
             "/var/lib/app/dumps");
}

TEST_F (LumexCoreDumpInstanceTest, GivenNoDirectory_WhenValueOr_ThenFallback)
{
  optional_dump_directory_t const directory
      = default_instance ().get_optional_dump_directory ();
  EXPECT_EQ (directory.value_or (std::string ("fallback")), "fallback");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenDirectory_WhenOptionalChanged_ThenInstanceKeepsItsDirectory)
{
  set_instance_directory ("/var/dumps");
  core_dump_generator const &generator = default_instance ();
  optional_dump_directory_t directory
      = generator.get_optional_dump_directory ();
  ASSERT_TRUE (directory.has_value ());
  *directory += "/changed";
  EXPECT_EQ (generator.get_instance_dump_directory (), "/var/dumps");
  optional_dump_directory_t const again
      = generator.get_optional_dump_directory ();
  ASSERT_TRUE (again.has_value ());
  EXPECT_EQ (*again, "/var/dumps");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenDirectoryChanged_WhenGetOptionalDumpDirectory_ThenFollows)
{
  core_dump_generator const &generator = default_instance ();
  EXPECT_FALSE (generator.get_optional_dump_directory ().has_value ());
  set_instance_directory ("/a");
  ASSERT_TRUE (generator.get_optional_dump_directory ().has_value ());
  EXPECT_EQ (*generator.get_optional_dump_directory (), "/a");
  set_instance_directory (std::string ());
  EXPECT_FALSE (generator.get_optional_dump_directory ().has_value ());
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenNoDirectory_WhenGetDumpDirectoryIfSet_ThenFalseAndOutputUntouched)
{
  std::string directory = "untouched";
  EXPECT_FALSE (default_instance ().get_dump_directory_if_set (directory));
  EXPECT_EQ (directory, "untouched");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenDirectory_WhenGetDumpDirectoryIfSet_ThenTrueAndOutputSet)
{
  set_instance_directory ("/var/dumps");
  std::string directory;
  EXPECT_TRUE (default_instance ().get_dump_directory_if_set (directory));
  EXPECT_EQ (directory, "/var/dumps");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenEitherDirectoryState_WhenBothGetters_ThenTheyAgree)
{
  core_dump_generator const &generator = default_instance ();
  for (std::string const &value : { std::string (), std::string ("/x") })
    {
      set_instance_directory (value);
      std::string out;
      bool const set = generator.get_dump_directory_if_set (out);
      optional_dump_directory_t const directory
          = generator.get_optional_dump_directory ();
      EXPECT_EQ (set, directory.has_value ());
      if (set)
        {
          EXPECT_EQ (out, *directory);
        }
    }
}

TEST (LumexCoreDumpInstanceTypeTest,
      GivenOptionalDumpDirectory_WhenInspected_ThenNoexceptConstAndTheAlias)
{
  static_assert (noexcept (std::declval<core_dump_generator const &> ()
                               .get_optional_dump_directory ()),
                 "get_optional_dump_directory is noexcept");
  static_assert (
      std::is_same<decltype (std::declval<core_dump_generator const &> ()
                                 .get_optional_dump_directory ()),
                   optional_dump_directory_t>::value,
      "the function returns the alias");
  static_assert (
      std::is_same<decltype (std::declval<core_dump_generator const &> ()
                                 .get_dump_directory_if_set (
                                     std::declval<std::string &> ())),
                   bool>::value,
      "get_dump_directory_if_set exists in every standard");
  SUCCEED ();
}

#if __cplusplus < 201703L

TEST (LumexCoreDumpInstanceTypeTest,
      GivenBeforeCxx17_WhenOptionalDumpDirectory_ThenTheOptionalOfTheLibrary)
{
  static_assert (
      std::is_same<optional_dump_directory_t,
                   lumex::core::optional::opt::optional<std::string>>::value,
      "the optional of this library before C++17");
  SUCCEED ();
}

TEST_F (
    LumexCoreDumpInstanceTest,
    GivenBeforeCxx17_WhenCompareWithNullopt_ThenOptionalOfTheLibraryBehaves)
{
  EXPECT_TRUE (default_instance ().get_optional_dump_directory ()
               == lumex::core::optional::opt::nullopt);
  set_instance_directory ("/d");
  EXPECT_FALSE (default_instance ().get_optional_dump_directory ()
                == lumex::core::optional::opt::nullopt);
}

#endif
