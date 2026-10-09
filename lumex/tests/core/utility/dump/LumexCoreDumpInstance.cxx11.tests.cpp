// LumexCoreDumpInstance.cxx11.tests.cpp
//
// The members of core_dump_generator that depend on the standard and that need
// state: get_memory_filters_range (static; the view of the filters of the
// current configuration), get_optional_dump_directory and
// get_dump_directory_if_set (the directory of an instance), and the template
// overloads of generate_instance_dump. They work from C++11: the range is the
// iterator_range of this library and the optional is the optional of this
// library in every standard (never std::ranges::ref_view or std::optional;
// the conversions and the std::ranges concepts are checked in
// LumexCoreDumpInstance.cxx17 and .cxx20; the behavior is run here, in every
// suite).
//
// There is no public way to put a filter into the static configuration or a
// directory into the state except initialize (), which installs signal
// handlers, rewrites the machine-wide core pattern through sudo and starts a
// monitor thread: not for a unit test. So the tests reach the private state
// the way a test can without changing the library, by naming the members in an
// explicit instantiation (which the access rules do not check): the static
// configuration and directory, the private "initialized" flag, the instance
// pointer and mutex, the members of an instance and the two private functions
// that fill an instance from the static state (_create_instance, which
// instance () calls once, and _refresh_instance, which initialize () and
// set_dump_type () call). initialize () writes the static state and calls
// _refresh_instance; the tests write the same members and call the same
// functions, which is everything of initialize () that concerns the instance.
// The fixture restores the static state before and after each test.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <string>
#if __cplusplus >= 201703L
#include <string_view>
#endif
#include <system_error>
#include <thread>
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
using lumex::core::utility::dump::DumpType;
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

struct static_directory_tag
{
  using type = std::string *;
};

struct instance_pointer_tag
{
  using type = std::unique_ptr<core_dump_generator> *;
};

struct static_mutex_tag
{
  using type = std::mutex *;
};

struct instance_configuration_tag
{
  using type = dump_configuration core_dump_generator::*;
};

struct instance_flag_tag
{
  using type = bool core_dump_generator::*;
};

struct create_instance_tag
{
  using type = void (*) ();
};

struct refresh_instance_tag
{
  using type = void (*) ();
};

template struct reveal<configuration_tag,
                       &core_dump_generator::s_currentConfig>;
template struct reveal<initialized_tag, &core_dump_generator::s_initialized>;
template struct reveal<directory_tag, &core_dump_generator::m_dumpDirectory>;
template struct reveal<static_directory_tag,
                       &core_dump_generator::s_dumpDirectory>;
template struct reveal<instance_pointer_tag, &core_dump_generator::s_instance>;
template struct reveal<static_mutex_tag, &core_dump_generator::s_mutex>;
template struct reveal<instance_configuration_tag,
                       &core_dump_generator::m_currentConfig>;
template struct reveal<instance_flag_tag,
                       &core_dump_generator::m_isInitialized>;
template struct reveal<create_instance_tag,
                       &core_dump_generator::_create_instance>;
template struct reveal<refresh_instance_tag,
                       &core_dump_generator::_refresh_instance>;

/// The shared instance (instance () creates it on the first call from the
/// static state): instance () refuses while the flag is false, so the flag is
/// true for the call only.
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

std::string &
static_directory ()
{
  return *slot<static_directory_tag>::value;
}

std::unique_ptr<core_dump_generator> &
instance_pointer ()
{
  return *slot<instance_pointer_tag>::value;
}

/// A fresh instance in s_instance for the scope, built by the function that
/// instance () calls (_create_instance), and the previous s_instance back
/// afterwards. A test that uses it must not call instance () itself: that
/// consumes the call_once of the real one.
class scoped_fresh_instance
{
public:
  scoped_fresh_instance ()
  {
    saved_.swap (instance_pointer ());
    slot<create_instance_tag>::value ();
  }

  ~scoped_fresh_instance () { instance_pointer ().swap (saved_); }

  scoped_fresh_instance (scoped_fresh_instance const &) = delete;
  scoped_fresh_instance &operator= (scoped_fresh_instance const &) = delete;

  core_dump_generator &
  get ()
  {
    return *instance_pointer ();
  }

  /// The instance as initialize () leaves it before anything is filled in:
  /// not initialized.
  void
  make_not_initialized ()
  {
    get ().*slot<instance_flag_tag>::value = false;
  }

private:
  std::unique_ptr<core_dump_generator> saved_;
};

/// The state initialize () writes, without installing anything.
void
write_static_state (std::string const &directory,
                    dump_configuration const &configuration)
{
  static_directory () = directory;
  current_configuration () = configuration;
}

/// Holds s_initialized true for the scope (set_dump_type () and the static
/// generate_dump () refuse while it is false).
class scoped_initialized_flag
{
public:
  scoped_initialized_flag ()
      : before_ (slot<initialized_tag>::value->exchange (true))
  {
  }

  ~scoped_initialized_flag ()
  {
    slot<initialized_tag>::value->store (before_);
  }

  scoped_initialized_flag (scoped_initialized_flag const &) = delete;
  scoped_initialized_flag &operator= (scoped_initialized_flag const &)
      = delete;

private:
  bool before_;
};

/// A dump type that this platform supports and that differs from the default.
DumpType
supported_type ()
{
  return LUMEX_OS_IS_WINDOWS () ? DumpType::MINI_DUMP_NORMAL
                                : DumpType::CORE_DUMP_FULL;
}

/// A dump type that this platform does not support.
DumpType
unsupported_type ()
{
  return LUMEX_OS_IS_WINDOWS () ? DumpType::CORE_DUMP_FULL
                                : DumpType::MINI_DUMP_NORMAL;
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

  void
  reset_state ()
  {
    if (!saved_)
      {
        saved_directory_ = static_directory ();
        saved_ = true;
      }
    current_configuration () = dump_configuration ();
    static_directory () = saved_directory_;
    set_instance_directory (std::string ());
  }

  std::string saved_directory_;
  bool saved_ = false;
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

// An instance that is not initialized never writes a dump: the call reports
// false (and invalid_argument with an error code). instance () no longer
// returns such an object (it fills it from the static state), so the tests
// take a fresh one and clear its flag. That makes the forwarding of every
// accepted argument type safe to run.

TEST (
    LumexCoreDumpInstanceGenerateTest,
    GivenUninitializedInstance_WhenGenerateInstanceDumpWithEveryType_ThenFalse)
{
  scoped_fresh_instance fresh;
  fresh.make_not_initialized ();
  core_dump_generator &generator = fresh.get ();
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
  scoped_fresh_instance fresh;
  fresh.make_not_initialized ();
  core_dump_generator &generator = fresh.get ();
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
  scoped_fresh_instance fresh;
  fresh.make_not_initialized ();
  core_dump_generator &generator = fresh.get ();
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

TEST (LumexCoreDumpInstanceTypeTest,
      GivenAnyStandard_WhenMemoryFiltersRange_ThenTheIteratorRangeOfTheLibrary)
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

// size () is in the iterator_range of the library for the random-access
// iterators of the filter list (std::ranges::ref_view, which the range used to
// be from C++20, has it too).
TEST_F (LumexCoreDumpInstanceTest,
        GivenFilters_WhenMemoryFiltersRange_ThenSizeIsTheCountOfFilters)
{
  EXPECT_EQ (core_dump_generator::get_memory_filters_range ().size (),
             static_cast<std::size_t> (0));
  ASSERT_TRUE (current_configuration ().add_memory_filter ("heap"));
  ASSERT_TRUE (current_configuration ().add_memory_filter ("stack"));
  EXPECT_EQ (core_dump_generator::get_memory_filters_range ().size (),
             static_cast<std::size_t> (2));
  ASSERT_TRUE (current_configuration ().add_memory_filter ("anon"));
  memory_filters_range_t const range
      = core_dump_generator::get_memory_filters_range ();
  EXPECT_EQ (range.size (), static_cast<std::size_t> (3));
  EXPECT_EQ (range.size (), walk (range).size ());
  current_configuration ().clear_memory_filters ();
  EXPECT_EQ (core_dump_generator::get_memory_filters_range ().size (),
             static_cast<std::size_t> (0));
}

TEST (LumexCoreDumpInstanceTypeTest,
      GivenMemoryFiltersRange_WhenInspected_ThenHasSizeInEveryStandard)
{
  static_assert (
      std::is_convertible<
          decltype (std::declval<memory_filters_range_t const &> ().size ()),
          std::size_t>::value,
      "the range has size () that converts to std::size_t");
  SUCCEED ();
}

// get_memory_filters_range forms the view while s_mutex is held, the mutex
// that initialize () and set_dump_type () hold while they replace the
// configuration. The test holds the mutex, calls the function on another
// thread and sees that the call waits; it completes once the mutex is free.
TEST_F (LumexCoreDumpInstanceTest,
        GivenStaticMutexHeld_WhenMemoryFiltersRange_ThenWaitsForIt)
{
  ASSERT_TRUE (current_configuration ().add_memory_filter ("heap"));
  std::mutex &mutex = *slot<static_mutex_tag>::value;
  std::promise<std::size_t> promise;
  std::future<std::size_t> future = promise.get_future ();
  std::thread worker;
  {
    std::unique_lock<std::mutex> lock (mutex);
    worker = std::thread (
        [&promise] ()
          {
            promise.set_value (
                core_dump_generator::get_memory_filters_range ().size ());
          });
    // A call that does not take the mutex returns at once; one that waits for
    // it cannot be ready while the mutex is held.
    EXPECT_EQ (future.wait_for (std::chrono::milliseconds (300)),
               std::future_status::timeout);
  }
  EXPECT_EQ (future.wait_for (std::chrono::seconds (30)),
             std::future_status::ready);
  worker.join ();
  EXPECT_EQ (future.get (), static_cast<std::size_t> (1));
}

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

TEST (LumexCoreDumpInstanceTypeTest,
      GivenAnyStandard_WhenOptionalDumpDirectory_ThenTheOptionalOfTheLibrary)
{
  static_assert (
      std::is_same<optional_dump_directory_t,
                   lumex::core::optional::opt::optional<std::string>>::value,
      "the optional of this library in every standard");
  SUCCEED ();
}

TEST_F (
    LumexCoreDumpInstanceTest,
    GivenAnyStandard_WhenCompareWithNullopt_ThenOptionalOfTheLibraryBehaves)
{
  EXPECT_TRUE (default_instance ().get_optional_dump_directory ()
               == lumex::core::optional::opt::nullopt);
  set_instance_directory ("/d");
  EXPECT_FALSE (default_instance ().get_optional_dump_directory ()
                == lumex::core::optional::opt::nullopt);
}

// --- the instance API after initialize () ---
//
// initialize () writes s_dumpDirectory and s_currentConfig, sets the static
// flag and calls _refresh_instance (); instance () creates the object with
// _create_instance (), which copies that state and marks the object
// initialized. Before this was fixed instance () built an empty, uninitialized
// object, so the directory getters were always empty and
// generate_instance_dump always returned false. The tests below write the same
// state (see the comment at the top) and call the same two functions.

namespace
{
dump_configuration
sample_configuration ()
{
  dump_configuration configuration;
  configuration.set_type (supported_type ());
  configuration.set_directory ("/var/lib/app/dumps");
  configuration.set_max_size_bytes (123456);
  configuration.add_memory_filter ("heap");
  configuration.add_memory_filter ("stack");
  return configuration;
}
} // namespace

TEST_F (LumexCoreDumpInstanceTest,
        GivenStaticState_WhenInstanceCreated_ThenItHoldsDirectoryConfigAndFlag)
{
  dump_configuration const configuration = sample_configuration ();
  write_static_state ("/var/lib/app/dumps", configuration);

  scoped_fresh_instance fresh;
  core_dump_generator const &generator = fresh.get ();

  EXPECT_TRUE (generator.is_instance_initialized ());
  EXPECT_EQ (generator.get_instance_dump_directory (), "/var/lib/app/dumps");
  EXPECT_TRUE (generator.get_instance_configuration () == configuration);
  EXPECT_EQ (generator.get_instance_configuration ().get_type (),
             supported_type ());
  EXPECT_EQ (generator.get_instance_configuration ().get_max_size_bytes (),
             static_cast<std::size_t> (123456));
  EXPECT_EQ (
      generator.get_instance_configuration ().get_memory_filters ().size (),
      static_cast<std::size_t> (2));
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenStaticState_WhenInstanceCreated_ThenOptionalDirectoryHoldsIt)
{
  write_static_state ("/var/lib/app/dumps", sample_configuration ());

  scoped_fresh_instance fresh;
  core_dump_generator const &generator = fresh.get ();

  optional_dump_directory_t const directory
      = generator.get_optional_dump_directory ();
  ASSERT_TRUE (directory.has_value ());
  EXPECT_EQ (*directory, "/var/lib/app/dumps");
  std::string out;
  EXPECT_TRUE (generator.get_dump_directory_if_set (out));
  EXPECT_EQ (out, "/var/lib/app/dumps");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenStaticState_WhenInstanceCreated_ThenAgreesWithTheStaticGetters)
{
  write_static_state ("/data/dumps", sample_configuration ());

  scoped_fresh_instance fresh;
  core_dump_generator const &generator = fresh.get ();

  EXPECT_EQ (generator.get_instance_dump_directory (),
             core_dump_generator::get_dump_directory ());
  EXPECT_TRUE (generator.get_instance_configuration ()
               == core_dump_generator::get_current_configuration ());
  EXPECT_EQ (generator.get_instance_configuration ().get_type (),
             core_dump_generator::get_current_dump_type ());
}

TEST_F (
    LumexCoreDumpInstanceTest,
    GivenEmptyStaticDirectory_WhenInstanceCreated_ThenInitializedWithNoDirectory)
{
  write_static_state (std::string (), dump_configuration ());

  scoped_fresh_instance fresh;
  core_dump_generator const &generator = fresh.get ();

  EXPECT_TRUE (generator.is_instance_initialized ());
  EXPECT_TRUE (generator.get_instance_dump_directory ().empty ());
  EXPECT_FALSE (generator.get_optional_dump_directory ().has_value ());
  std::string out = "untouched";
  EXPECT_FALSE (generator.get_dump_directory_if_set (out));
  EXPECT_EQ (out, "untouched");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenInstanceCreated_WhenStaticStateChanges_ThenItKeepsItsCopy)
{
  write_static_state ("/first", sample_configuration ());
  scoped_fresh_instance fresh;
  core_dump_generator const &generator = fresh.get ();

  write_static_state ("/second", dump_configuration ());

  EXPECT_EQ (generator.get_instance_dump_directory (), "/first");
  EXPECT_TRUE (generator.get_instance_configuration ()
               == sample_configuration ());
}

// --- _refresh_instance, the step initialize () and set_dump_type () take ---

TEST_F (LumexCoreDumpInstanceTest,
        GivenInstanceCreated_WhenRefreshed_ThenFollowsTheStaticState)
{
  write_static_state ("/first", sample_configuration ());
  scoped_fresh_instance fresh;
  core_dump_generator const &generator = fresh.get ();

  dump_configuration other;
  other.set_type (DumpType::DEFAULT_AUTO);
  other.set_max_size_bytes (7);
  write_static_state ("/second", other);
  slot<refresh_instance_tag>::value ();

  EXPECT_EQ (generator.get_instance_dump_directory (), "/second");
  EXPECT_TRUE (generator.get_instance_configuration () == other);
  EXPECT_TRUE (generator.is_instance_initialized ());
  EXPECT_EQ (
      generator.get_optional_dump_directory ().value_or (std::string ("none")),
      "/second");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenNotInitializedInstance_WhenRefreshed_ThenBecomesInitialized)
{
  write_static_state ("/dir", sample_configuration ());
  scoped_fresh_instance fresh;
  fresh.make_not_initialized ();
  ASSERT_FALSE (fresh.get ().is_instance_initialized ());

  slot<refresh_instance_tag>::value ();

  EXPECT_TRUE (fresh.get ().is_instance_initialized ());
  EXPECT_EQ (fresh.get ().get_instance_dump_directory (), "/dir");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenNoInstanceYet_WhenRefreshed_ThenNothingHappens)
{
  std::unique_ptr<core_dump_generator> saved;
  saved.swap (instance_pointer ());
  write_static_state ("/dir", sample_configuration ());

  slot<refresh_instance_tag>::value ();

  EXPECT_TRUE (instance_pointer () == nullptr);
  instance_pointer ().swap (saved);
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenStaticMutexHeld_WhenRefreshed_ThenDoesNotLockItAgain)
{
  // initialize () and set_dump_type () call _refresh_instance () with s_mutex
  // held; locking it again would deadlock, so the call must return.
  write_static_state ("/dir", sample_configuration ());
  scoped_fresh_instance fresh;
  std::lock_guard<std::mutex> lock (*slot<static_mutex_tag>::value);
  slot<refresh_instance_tag>::value ();
  EXPECT_EQ (fresh.get ().get_instance_dump_directory (), "/dir");
}

// --- set_dump_type () refreshes the instance ---
//
// set_dump_type () installs nothing: it checks the flag, the platform support
// and replaces the static configuration (it keeps the directory).

TEST_F (LumexCoreDumpInstanceTest,
        GivenInstance_WhenSetDumpType_ThenInstanceConfigurationFollows)
{
  write_static_state ("/keep/this", dump_configuration ());
  scoped_fresh_instance fresh;
  core_dump_generator const &generator = fresh.get ();
  ASSERT_EQ (generator.get_instance_configuration ().get_type (),
             DumpType::DEFAULT_AUTO);

  scoped_initialized_flag initialized;
  ASSERT_TRUE (core_dump_generator::set_dump_type (supported_type ()));

  EXPECT_EQ (core_dump_generator::get_current_dump_type (), supported_type ());
  EXPECT_EQ (generator.get_instance_configuration ().get_type (),
             supported_type ());
  EXPECT_TRUE (generator.get_instance_configuration ()
               == core_dump_generator::get_current_configuration ());
  EXPECT_EQ (generator.get_instance_dump_directory (), "/keep/this");
  EXPECT_EQ (generator.get_instance_configuration ().get_directory (),
             "/keep/this");
  EXPECT_TRUE (generator.is_instance_initialized ());
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenInstance_WhenSetDumpTypeUnsupported_ThenInstanceUnchanged)
{
  write_static_state ("/keep/this", sample_configuration ());
  scoped_fresh_instance fresh;
  core_dump_generator const &generator = fresh.get ();

  scoped_initialized_flag initialized;
  EXPECT_FALSE (core_dump_generator::set_dump_type (unsupported_type ()));

  EXPECT_TRUE (generator.get_instance_configuration ()
               == sample_configuration ());
  EXPECT_EQ (generator.get_instance_dump_directory (), "/keep/this");
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenNoInstanceYet_WhenSetDumpType_ThenStaticConfigurationChanges)
{
  std::unique_ptr<core_dump_generator> saved;
  saved.swap (instance_pointer ());
  write_static_state ("/dir", dump_configuration ());

  {
    scoped_initialized_flag initialized;
    EXPECT_TRUE (core_dump_generator::set_dump_type (supported_type ()));
  }

  EXPECT_EQ (core_dump_generator::get_current_dump_type (), supported_type ());
  EXPECT_TRUE (instance_pointer () == nullptr);
  instance_pointer ().swap (saved);
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenNotInitialized_WhenSetDumpType_ThenThrowsAndNothingChanges)
{
  write_static_state ("/dir", dump_configuration ());
  EXPECT_THROW (core_dump_generator::set_dump_type (supported_type ()),
                std::runtime_error);
  EXPECT_EQ (core_dump_generator::get_current_dump_type (),
             DumpType::DEFAULT_AUTO);
}

// --- instance () ---

TEST (LumexCoreDumpInstanceCreationTest,
      GivenNotInitialized_WhenInstance_ThenThrows)
{
  ASSERT_FALSE (core_dump_generator::is_initialized ());
  EXPECT_THROW (core_dump_generator::instance (), std::runtime_error);
}

TEST_F (LumexCoreDumpInstanceTest,
        GivenInitialized_WhenInstance_ThenFilledAndTheSameEveryTime)
{
  write_static_state ("/var/dumps", sample_configuration ());
  core_dump_generator &first = default_instance ();
  core_dump_generator &second = default_instance ();
  EXPECT_EQ (&first, &second);
  EXPECT_EQ (&first, instance_pointer ().get ());
  // Whenever instance () created the object (this test or an earlier one),
  // it filled it; a refresh makes it follow this test's state.
  EXPECT_TRUE (first.is_instance_initialized ());
  slot<refresh_instance_tag>::value ();
  EXPECT_EQ (first.get_instance_dump_directory (), "/var/dumps");
  EXPECT_TRUE (first.get_instance_configuration () == sample_configuration ());
}

// --- generate_instance_dump () with a filled instance ---
//
// A filled instance hands the call to the static generate_dump (); an unfilled
// one stops at its own guard. With the static state not initialized the static
// function answers operation_not_permitted, the guard of the instance answers
// invalid_argument, so the two paths can be told apart without writing a dump.

TEST_F (
    LumexCoreDumpInstanceTest,
    GivenFilledInstance_WhenGenerateInstanceDumpWithErrorCode_ThenReachesTheStaticFunction)
{
  write_static_state ("/dir", sample_configuration ());
  scoped_fresh_instance fresh;
  ASSERT_TRUE (fresh.get ().is_instance_initialized ());
  ASSERT_FALSE (core_dump_generator::is_initialized ());

  std::error_code code;
  EXPECT_FALSE (fresh.get ().generate_instance_dump ("reason", code));
  EXPECT_TRUE (code == std::errc::operation_not_permitted);

  code.clear ();
  EXPECT_FALSE (fresh.get ().generate_instance_dump (std::string ("x"), code));
  EXPECT_TRUE (code == std::errc::operation_not_permitted);
}

TEST_F (
    LumexCoreDumpInstanceTest,
    GivenUnfilledInstance_WhenGenerateInstanceDumpWithErrorCode_ThenStopsAtItsGuard)
{
  write_static_state ("/dir", sample_configuration ());
  scoped_fresh_instance fresh;
  fresh.make_not_initialized ();

  std::error_code code;
  EXPECT_FALSE (fresh.get ().generate_instance_dump ("reason", code));
  EXPECT_TRUE (code == std::errc::invalid_argument);
}

TEST_F (
    LumexCoreDumpInstanceTest,
    GivenFilledInstanceOfAnUnsupportedType_WhenGenerateInstanceDump_ThenRejectedWithoutADump)
{
  dump_configuration configuration;
  configuration.set_type (unsupported_type ());
  write_static_state ("/dir", configuration);
  scoped_fresh_instance fresh;
  scoped_initialized_flag initialized;

  std::error_code code;
  EXPECT_FALSE (fresh.get ().generate_instance_dump ("reason", code));
  EXPECT_TRUE (code == std::errc::invalid_argument);
  // The overload without an error code is not called here: it does not check
  // the platform support and would go on to write a real dump.
}
