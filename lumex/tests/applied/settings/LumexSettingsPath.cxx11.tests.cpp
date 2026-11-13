// lumex/tests/applied/settings/LumexSettingsPath.cxx11.tests.cpp
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/applied/settings/LumexSettings"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/tests/support/LumexTestNarrowSkip.hpp"
#include "lumex/tests/support/LumexTestUtf8Files.hpp"

using namespace lumex::applied::settings;
using lumex::core::filesystem::fs::lumex_filesystem;

namespace
{
// --- Detectors ---

// True when `settings.load (argument)` is well-formed and unambiguous for an
// object of type `Settings` and an argument of type `Arg`.
template <typename Settings, typename Arg> struct can_load_t
{
private:
  template <typename S, typename A>
  static auto probe (int)
      -> decltype (std::declval<S &> ().load (std::declval<A> ()),
                   std::true_type ());
  template <typename S, typename A> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Settings, Arg> (0))::value;
};

// The same for `save`, which is a const member function.
template <typename Settings, typename Arg> struct can_save_t
{
private:
  template <typename S, typename A>
  static auto probe (int)
      -> decltype (std::declval<S const &> ().save (std::declval<A> ()),
                   std::true_type ());
  template <typename S, typename A> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Settings, Arg> (0))::value;
};

// True when the address of an overloaded member can be passed to a function
// template that deduces its type (`auto pm = &ILumexSettings::load;`).
template <typename F> void take_by_deduction (F);

template <typename Base> struct can_deduce_load_address_t
{
private:
  template <typename B>
  static auto probe (int)
      -> decltype (take_by_deduction (&B::load), std::true_type ());
  template <typename B> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Base> (0))::value;
};

template <typename Base> struct can_deduce_save_address_t
{
private:
  template <typename B>
  static auto probe (int)
      -> decltype (take_by_deduction (&B::save), std::true_type ());
  template <typename B> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Base> (0))::value;
};

// Types the `lumex::path` form must not accept.
struct convertible_to_path_t
{
  operator lumex::path () const; // declared only: never called
};

struct derived_path_t : lumex::path
{
};

// A string literal, as `load ("x.ini")` passes it.
using literal_t = char const (&)[6];

// --- Doubles ---

// Records the strings that reach the virtual functions. The using-declarations
// bring the `lumex::path` forms of `ILumexSettings` back into the class.
class recording_settings_t : public ILumexSettings
{
public:
  using ILumexSettings::load;
  using ILumexSettings::save;

  int loads = 0;
  mutable int saves = 0;
  std::string last_load;
  mutable std::string last_save;

  bool
  load (std::string const &path) override
  {
    ++loads;
    last_load = path;
    return true;
  }

  bool
  save (std::string const &path) const override
  {
    ++saves;
    last_save = path;
    return true;
  }

  std::string
  get (std::string const &, std::string const &) const override
  {
    return std::string ();
  }

  void
  add (std::string const &, std::string const &, std::string const &) override
  {
  }

  void
  remove (std::string const &, std::string const &) override
  {
  }
};

// The same without the using-declarations: the overrides hide the templates.
class hiding_settings_t : public ILumexSettings
{
public:
  int loads = 0;
  mutable int saves = 0;
  std::string last_load;
  mutable std::string last_save;

  bool
  load (std::string const &path) override
  {
    ++loads;
    last_load = path;
    return true;
  }

  bool
  save (std::string const &path) const override
  {
    ++saves;
    last_save = path;
    return true;
  }

  std::string
  get (std::string const &, std::string const &) const override
  {
    return std::string ();
  }

  void
  add (std::string const &, std::string const &, std::string const &) override
  {
  }

  void
  remove (std::string const &, std::string const &) override
  {
  }
};

// --- Backends ---

struct ini_backend_t
{
  static char const *
  name ()
  {
    return "INI";
  }
  using settings_t = LumexSettingsINI;
  static LumexSettingsExtensions
  extension ()
  {
    return LumexSettingsExtensions::INI;
  }
  static char const *
  suffix ()
  {
    return ".ini";
  }
};

struct json_backend_t
{
  static char const *
  name ()
  {
    return "JSON";
  }
  using settings_t = LumexSettingsJSON;
  static LumexSettingsExtensions
  extension ()
  {
    return LumexSettingsExtensions::JSON;
  }
  static char const *
  suffix ()
  {
    return ".json";
  }
};

struct xml_backend_t
{
  static char const *
  name ()
  {
    return "XML";
  }
  using settings_t = LumexSettingsXML;
  static LumexSettingsExtensions
  extension ()
  {
    return LumexSettingsExtensions::XML;
  }
  static char const *
  suffix ()
  {
    return ".xml";
  }
};

char const *const SECTION = "pump";
char const *const KEY = "flow";
char const *const VALUE = "1.5";
} // namespace

// --- Form of the overloads (compile time) ---
// --------------------------------

TEST (LumexSettingsPathForm,
      GivenEveryOverload_WhenCalledThroughTheInterface_ThenWellFormed)
{
  static_assert (can_load_t<ILumexSettings, literal_t>::value,
                 "load (\"x.ini\") through the interface");
  static_assert (can_load_t<ILumexSettings, char const *>::value,
                 "load (char const *) through the interface");
  static_assert (can_load_t<ILumexSettings, std::string>::value,
                 "load (std::string) through the interface");
  static_assert (can_load_t<ILumexSettings, std::string const &>::value,
                 "load (std::string const &) through the interface");
  static_assert (can_load_t<ILumexSettings, lumex::path>::value,
                 "load (lumex::path) through the interface");
  static_assert (can_load_t<ILumexSettings, lumex::path &>::value,
                 "load (lumex::path &) through the interface");
  static_assert (can_load_t<ILumexSettings, lumex::path const &>::value,
                 "load (lumex::path const &) through the interface");

  static_assert (can_save_t<ILumexSettings, literal_t>::value,
                 "save (\"x.ini\") through the interface");
  static_assert (can_save_t<ILumexSettings, std::string>::value,
                 "save (std::string) through the interface");
  static_assert (can_save_t<ILumexSettings, lumex::path>::value,
                 "save (lumex::path) through the interface");
  static_assert (can_save_t<ILumexSettings, lumex::path const &>::value,
                 "save (lumex::path const &) through the interface");
  SUCCEED ();
}

TEST (LumexSettingsPathForm,
      GivenEveryOverload_WhenCalledThroughTheBackends_ThenWellFormed)
{
  static_assert (can_load_t<LumexSettingsINI, literal_t>::value,
                 "INI literal");
  static_assert (can_load_t<LumexSettingsINI, std::string>::value,
                 "INI string");
  static_assert (can_load_t<LumexSettingsINI, lumex::path>::value, "INI path");
  static_assert (can_save_t<LumexSettingsINI, literal_t>::value,
                 "INI literal");
  static_assert (can_save_t<LumexSettingsINI, std::string>::value,
                 "INI string");
  static_assert (can_save_t<LumexSettingsINI, lumex::path>::value, "INI path");

  static_assert (can_load_t<LumexSettingsJSON, literal_t>::value,
                 "JSON literal");
  static_assert (can_load_t<LumexSettingsJSON, std::string>::value,
                 "JSON string");
  static_assert (can_load_t<LumexSettingsJSON, lumex::path>::value,
                 "JSON path");
  static_assert (can_save_t<LumexSettingsJSON, literal_t>::value,
                 "JSON literal");
  static_assert (can_save_t<LumexSettingsJSON, std::string>::value,
                 "JSON string");
  static_assert (can_save_t<LumexSettingsJSON, lumex::path>::value,
                 "JSON path");

  static_assert (can_load_t<LumexSettingsXML, literal_t>::value,
                 "XML literal");
  static_assert (can_load_t<LumexSettingsXML, std::string>::value,
                 "XML string");
  static_assert (can_load_t<LumexSettingsXML, lumex::path>::value, "XML path");
  static_assert (can_save_t<LumexSettingsXML, literal_t>::value,
                 "XML literal");
  static_assert (can_save_t<LumexSettingsXML, std::string>::value,
                 "XML string");
  static_assert (can_save_t<LumexSettingsXML, lumex::path>::value, "XML path");
  SUCCEED ();
}

TEST (LumexSettingsPathForm,
      GivenTypeThatOnlyConvertsToPath_WhenCalled_ThenTheTemplateIsNotSelected)
{
  static_assert (!can_load_t<ILumexSettings, convertible_to_path_t>::value,
                 "a type convertible to lumex::path selects no overload");
  static_assert (!can_save_t<ILumexSettings, convertible_to_path_t>::value,
                 "a type convertible to lumex::path selects no overload");
  static_assert (!can_load_t<LumexSettingsINI, convertible_to_path_t>::value,
                 "a type convertible to lumex::path selects no overload");
  static_assert (!can_save_t<LumexSettingsXML, convertible_to_path_t>::value,
                 "a type convertible to lumex::path selects no overload");

  // Exactly lumex::path: a class derived from it is another type.
  static_assert (!can_load_t<ILumexSettings, derived_path_t>::value,
                 "a class derived from lumex::path is not lumex::path");
  static_assert (!can_save_t<ILumexSettings, derived_path_t const &>::value,
                 "a class derived from lumex::path is not lumex::path");

  // Other types that never named a file.
  static_assert (!can_load_t<ILumexSettings, int>::value, "int");
  static_assert (!can_load_t<ILumexSettings, std::wstring>::value, "wstring");
  static_assert (!can_save_t<ILumexSettings, std::wstring>::value, "wstring");
  SUCCEED ();
}

TEST (
    LumexSettingsPathForm,
    GivenDerivedClassWithAndWithoutUsing_WhenPathFormCalled_ThenOnlyUsingKeepsIt)
{
  // With the using-declarations the derived type keeps the path form.
  static_assert (can_load_t<recording_settings_t, lumex::path>::value,
                 "using ILumexSettings::load keeps the path form");
  static_assert (can_save_t<recording_settings_t, lumex::path>::value,
                 "using ILumexSettings::save keeps the path form");

  // Without them the overrides hide the templates (documented), while the
  // string forms are untouched.
  static_assert (!can_load_t<hiding_settings_t, lumex::path>::value,
                 "the override hides the path form of load");
  static_assert (!can_save_t<hiding_settings_t, lumex::path>::value,
                 "the override hides the path form of save");
  static_assert (can_load_t<hiding_settings_t, std::string>::value,
                 "the string form is not hidden");
  static_assert (can_load_t<hiding_settings_t, literal_t>::value,
                 "the literal form is not hidden");
  static_assert (can_save_t<hiding_settings_t, std::string>::value,
                 "the string form is not hidden");

  // Through the interface the path form is always there.
  static_assert (can_load_t<ILumexSettings, lumex::path>::value,
                 "the interface keeps the path form");
  SUCCEED ();
}

TEST (LumexSettingsPathForm,
      GivenOverloadedNames_WhenAddressTakenWithTargetType_ThenStillWorks)
{
  bool (ILumexSettings::*load_pointer) (std::string const &)
      = &ILumexSettings::load;
  bool (ILumexSettings::*save_pointer) (std::string const &) const
      = &ILumexSettings::save;
  auto const cast_load
      = static_cast<bool (ILumexSettings::*) (std::string const &)> (
          &ILumexSettings::load);
  auto const cast_save
      = static_cast<bool (ILumexSettings::*) (std::string const &) const> (
          &ILumexSettings::save);

  recording_settings_t settings;
  ILumexSettings &base = settings;
  EXPECT_TRUE ((base.*load_pointer) ("by pointer.ini"));
  EXPECT_TRUE ((base.*save_pointer) ("by pointer.cfg"));
  EXPECT_TRUE ((base.*cast_load) ("by cast.ini"));
  EXPECT_TRUE ((base.*cast_save) ("by cast.cfg"));
  EXPECT_EQ (settings.loads, 2);
  EXPECT_EQ (settings.saves, 2);
  EXPECT_EQ (settings.last_load, "by cast.ini");
  EXPECT_EQ (settings.last_save, "by cast.cfg");
}

TEST (LumexSettingsPathForm,
      GivenOverloadedNames_WhenAddressTakenByDeduction_ThenIllFormed)
{
  // The source break of the member templates, documented: a deduced type
  // (`auto pm = &ILumexSettings::load;`, `std::bind`, `std::mem_fn`) no longer
  // compiles; a target type or a `static_cast` does.
  static_assert (!can_deduce_load_address_t<ILumexSettings>::value,
                 "the address of load is overloaded now");
  static_assert (!can_deduce_save_address_t<ILumexSettings>::value,
                 "the address of save is overloaded now");
  SUCCEED ();
}

// --- What reaches the virtual functions ---
// --------------------------------------

TEST (LumexSettingsPathForward,
      GivenPath_WhenLoadAndSaveThroughDerivedType_ThenVirtualGetsTheUtf8Text)
{
  recording_settings_t settings;
  std::string const text
      = lumex_test::utf8_files::cyrillic (" dir/file name.ini");
  lumex::path const file (text);

  EXPECT_TRUE (settings.load (file));
  EXPECT_TRUE (settings.save (file));

  EXPECT_EQ (settings.loads, 1);
  EXPECT_EQ (settings.saves, 1);
  EXPECT_EQ (settings.last_load, file.u8string ());
  EXPECT_EQ (settings.last_save, file.u8string ());
  EXPECT_EQ (settings.last_load, text);
}

TEST (LumexSettingsPathForward,
      GivenPath_WhenLoadAndSaveThroughInterface_ThenOverrideOfHidingClassRuns)
{
  hiding_settings_t settings;
  ILumexSettings &interface_ref = settings;
  lumex::path const file ("dir/file.ini");

  EXPECT_TRUE (interface_ref.load (file));
  EXPECT_TRUE (interface_ref.save (file));

  EXPECT_EQ (settings.loads, 1);
  EXPECT_EQ (settings.saves, 1);
  EXPECT_EQ (settings.last_load, "dir/file.ini");
  EXPECT_EQ (settings.last_save, "dir/file.ini");
}

TEST (LumexSettingsPathForward,
      GivenPathTemporary_WhenLoadAndSave_ThenSameAsLvalue)
{
  recording_settings_t settings;

  EXPECT_TRUE (settings.load (lumex::path ("a/b.ini")));
  EXPECT_TRUE (settings.save (lumex::path ("c/d.ini")));

  EXPECT_EQ (settings.last_load, "a/b.ini");
  EXPECT_EQ (settings.last_save, "c/d.ini");
}

TEST (LumexSettingsPathForward,
      GivenLiteralAndString_WhenLoadAndSave_ThenTheVirtualFormIsCalledAsBefore)
{
  recording_settings_t settings;
  std::string const text ("from string.ini");

  EXPECT_TRUE (settings.load ("x.ini"));
  EXPECT_EQ (settings.last_load, "x.ini");
  EXPECT_TRUE (settings.load (text));
  EXPECT_EQ (settings.last_load, text);
  EXPECT_TRUE (settings.save ("y.ini"));
  EXPECT_EQ (settings.last_save, "y.ini");
  EXPECT_TRUE (settings.save (text));
  EXPECT_EQ (settings.last_save, text);
  EXPECT_EQ (settings.loads, 2);
  EXPECT_EQ (settings.saves, 2);

  hiding_settings_t hiding;
  EXPECT_TRUE (hiding.load ("x.ini"));
  EXPECT_EQ (hiding.last_load, "x.ini");
  EXPECT_TRUE (hiding.save (text));
  EXPECT_EQ (hiding.last_save, text);
}

TEST (LumexSettingsPathForward, GivenEmptyPath_WhenLoad_ThenEmptyTextReaches)
{
  recording_settings_t settings;
  settings.last_load = "stale";

  EXPECT_TRUE (settings.load (lumex::path ()));

  EXPECT_EQ (settings.loads, 1);
  EXPECT_TRUE (settings.last_load.empty ());
}

// --- The backends through lumex::path ---
// ----------------------------------------

template <typename Backend>
class LumexSettingsPathTest : public ::testing::Test
{
protected:
  lumex::path _dir;

  void
  SetUp () override
  {
    if (!LumexSettingsFactory::create (Backend::extension ()))
      GTEST_SKIP () << "the build does not provide this format";
    ::testing::TestInfo const *info
        = ::testing::UnitTest::GetInstance ()->current_test_info ();
    std::string name = "test_settings_path_";
    name += info->test_suite_name ();
    name += "_";
    name += info->name ();
    for (auto &chr : name)
      if (chr == '/')
        chr = '_';
    _dir = lumex::path (name);
    remove_directory ();
    ASSERT_TRUE (lumex_filesystem::create_directories (_dir).success ());
  }

  void
  TearDown () override
  {
    remove_directory ();
  }

  // A directory and a file name with Cyrillic letters and a space.
  lumex::path
  nested_file () const
  {
    namespace files = lumex_test::utf8_files;
    return _dir / files::cyrillic (" dir name")
           / files::cyrillic (std::string (" file") + Backend::suffix ());
  }

  static void
  fill (typename Backend::settings_t &settings)
  {
    settings.add (SECTION, KEY, VALUE);
  }

private:
  void
  remove_directory () const
  {
    if (_dir.empty () || !lumex_filesystem::exists (_dir))
      return;
    if (!lumex_filesystem::remove_all (_dir).success ())
      ADD_FAILURE () << "cannot remove " << _dir.string ();
  }
};

using Backends
    = ::testing::Types<ini_backend_t, json_backend_t, xml_backend_t>;

// Names the gtest instances after the format (LumexSettingsPathTest/INI);
// the CTest names do not carry it.
struct backend_name_t
{
  template <typename Backend>
  static std::string
  GetName (int)
  {
    return Backend::name ();
  }
};

TYPED_TEST_SUITE (LumexSettingsPathTest, Backends, backend_name_t);

TYPED_TEST (
    LumexSettingsPathTest,
    GivenCyrillicPathWithSpace_WhenSavedByPathAndLoadedByString_ThenValueRoundTrips)
{
  LUMEX_TEST_SKIP_WITHOUT_NARROW_UTF8_NAMES ();
  namespace files = lumex_test::utf8_files;
  lumex::path const file = this->nested_file ();
  ASSERT_TRUE (files::make_directory (file.parent_path ().string ()));

  typename TypeParam::settings_t writer;
  this->fill (writer);
  ASSERT_TRUE (writer.save (file));
  EXPECT_TRUE (files::exists (file.string ()));
  EXPECT_TRUE (lumex_filesystem::exists (file));

  typename TypeParam::settings_t reader;
  ASSERT_TRUE (reader.load (file.string ()));
  EXPECT_EQ (reader.get (SECTION, KEY), VALUE);
}

TYPED_TEST (
    LumexSettingsPathTest,
    GivenCyrillicPathWithSpace_WhenSavedByStringAndLoadedByPath_ThenValueRoundTrips)
{
  LUMEX_TEST_SKIP_WITHOUT_NARROW_UTF8_NAMES ();
  namespace files = lumex_test::utf8_files;
  lumex::path const file = this->nested_file ();
  ASSERT_TRUE (files::make_directory (file.parent_path ().string ()));

  typename TypeParam::settings_t writer;
  this->fill (writer);
  ASSERT_TRUE (writer.save (file.string ()));

  typename TypeParam::settings_t reader;
  ASSERT_TRUE (reader.load (file));
  EXPECT_EQ (reader.get (SECTION, KEY), VALUE);
}

TYPED_TEST (LumexSettingsPathTest,
            GivenPathThroughInterface_WhenSavedAndLoaded_ThenValueRoundTrips)
{
  LUMEX_TEST_SKIP_WITHOUT_NARROW_UTF8_NAMES ();
  namespace files = lumex_test::utf8_files;
  lumex::path const file = this->nested_file ();
  ASSERT_TRUE (files::make_directory (file.parent_path ().string ()));

  std::unique_ptr<ILumexSettings> writer
      = LumexSettingsFactory::create (TypeParam::extension ());
  ASSERT_NE (writer, nullptr);
  writer->add (SECTION, KEY, VALUE);
  ASSERT_TRUE (writer->save (file));

  std::unique_ptr<ILumexSettings> reader
      = LumexSettingsFactory::create (TypeParam::extension ());
  ASSERT_NE (reader, nullptr);
  ASSERT_TRUE (reader->load (file));
  EXPECT_EQ (reader->get (SECTION, KEY), VALUE);
}

TYPED_TEST (
    LumexSettingsPathTest,
    GivenPlainPath_WhenSavedByPath_ThenTheFileIsAtThePathOfTheStringForm)
{
  lumex::path const file
      = this->_dir / (std::string ("plain") + TypeParam::suffix ());

  typename TypeParam::settings_t writer;
  this->fill (writer);
  ASSERT_TRUE (writer.save (file));
  EXPECT_TRUE (lumex_filesystem::exists (file));

  // The same file is the one the string form reads, and a second save through
  // the string form replaces it.
  typename TypeParam::settings_t reader;
  ASSERT_TRUE (reader.load (file.string ()));
  EXPECT_EQ (reader.get (SECTION, KEY), VALUE);
  reader.add (SECTION, KEY, "2.5");
  ASSERT_TRUE (reader.save (file.string ()));

  typename TypeParam::settings_t again;
  ASSERT_TRUE (again.load (file));
  EXPECT_EQ (again.get (SECTION, KEY), "2.5");
}

TYPED_TEST (LumexSettingsPathTest,
            GivenMissingFile_WhenLoadByPath_ThenReturnsFalse)
{
  typename TypeParam::settings_t reader;

  EXPECT_FALSE (reader.load (this->_dir / "absent"));
  EXPECT_FALSE (reader.load ((this->_dir / "absent").string ()));
}
