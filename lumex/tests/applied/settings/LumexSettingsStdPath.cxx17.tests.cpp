// lumex/tests/applied/settings/LumexSettingsStdPath.cxx17.tests.cpp
#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

#include "lumex/applied/settings/LumexSettings"
#include "lumex/core/filesystem/LumexFilesystem"

using namespace lumex::applied::settings;

namespace
{
// A `std::filesystem::path` is not a `lumex::path`, so the `lumex::path`
// templates do not take it. On POSIX it still reaches the `std::string` form
// through its own conversion to `std::string`, as before the templates; on
// Windows that conversion yields a `std::wstring` and the call does not
// compile.
#if defined(_WIN32)
constexpr bool STD_PATH_REACHES_THE_STRING_FORM = false;
#else
constexpr bool STD_PATH_REACHES_THE_STRING_FORM = true;
#endif

using std_path_t = std::filesystem::path;

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

template <typename Arg> struct can_check_ini_t
{
private:
  template <typename A>
  static auto probe (int)
      -> decltype (LumexSettingsINI::is_ini_valid (std::declval<A> ()),
                   std::true_type ());
  template <typename A> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Arg> (0))::value;
};

template <typename Arg> struct can_check_json_t
{
private:
  template <typename A>
  static auto probe (int)
      -> decltype (LumexSettingsJSON::is_json_valid (std::declval<A> ()),
                   std::true_type ());
  template <typename A> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Arg> (0))::value;
};

template <typename Arg> struct can_check_xml_t
{
private:
  template <typename A>
  static auto probe (int)
      -> decltype (LumexSettingsXML::is_xml_valid (std::declval<A> ()),
                   std::true_type ());
  template <typename A> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Arg> (0))::value;
};

template <typename Arg> struct can_back_up_t
{
private:
  template <typename A>
  static auto probe (int)
      -> decltype (LumexSettingsGuard::backup (std::declval<A> ()),
                   std::true_type ());
  template <typename A> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Arg> (0))::value;
};

template <typename Arg> struct can_construct_guard_t
{
private:
  template <typename A>
  static auto probe (int)
      -> decltype (LumexSettingsGuard (
                       std::declval<std::shared_ptr<ILumexSettings>> (),
                       std::declval<A> ()),
                   std::true_type ());
  template <typename A> static std::false_type probe (...);

public:
  static constexpr bool value = decltype (probe<Arg> (0))::value;
};

// Records the strings that reach the virtual functions.
class recording_settings_t : public ILumexSettings
{
public:
  using ILumexSettings::load;
  using ILumexSettings::save;

  std::string last_load;
  mutable std::string last_save;

  bool
  load (std::string const &path) override
  {
    last_load = path;
    return true;
  }

  bool
  save (std::string const &path) const override
  {
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
} // namespace

TEST (
    LumexSettingsStdPathForm,
    GivenStdFilesystemPath_WhenLoadOrSave_ThenOnlyThePlatformsStringFormTakesIt)
{
  static_assert (can_load_t<ILumexSettings, std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "load through the interface");
  static_assert (can_load_t<ILumexSettings, std_path_t const &>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "load through the interface, const reference");
  static_assert (can_save_t<ILumexSettings, std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "save through the interface");
  static_assert (can_load_t<LumexSettingsINI, std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "INI load");
  static_assert (can_save_t<LumexSettingsINI, std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "INI save");
  static_assert (can_load_t<LumexSettingsJSON, std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "JSON load");
  static_assert (can_save_t<LumexSettingsJSON, std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "JSON save");
  static_assert (can_load_t<LumexSettingsXML, std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "XML load");
  static_assert (can_save_t<LumexSettingsXML, std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "XML save");
  SUCCEED ();
}

TEST (
    LumexSettingsStdPathForm,
    GivenStdFilesystemPath_WhenValidatedOrBackedUp_ThenOnlyThePlatformsStringFormTakesIt)
{
  static_assert (can_check_ini_t<std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "is_ini_valid");
  static_assert (can_check_json_t<std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "is_json_valid");
  static_assert (can_check_xml_t<std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "is_xml_valid");
  static_assert (can_back_up_t<std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "backup");
  static_assert (can_construct_guard_t<std_path_t>::value
                     == STD_PATH_REACHES_THE_STRING_FORM,
                 "the guard constructor");
  SUCCEED ();
}

TEST (
    LumexSettingsStdPathForm,
    GivenStdFilesystemPathConvertedToLumexPath_WhenAnyForm_ThenWellFormedEverywhere)
{
  // The documented way: `lumex::path (std_path)` works on every platform.
  static_assert (std::is_constructible<lumex::path, std_path_t>::value,
                 "lumex::path (std_path)");
  static_assert (can_load_t<ILumexSettings, lumex::path>::value, "load");
  static_assert (can_save_t<ILumexSettings, lumex::path>::value, "save");
  static_assert (can_check_ini_t<lumex::path>::value, "is_ini_valid");
  static_assert (can_check_json_t<lumex::path>::value, "is_json_valid");
  static_assert (can_check_xml_t<lumex::path>::value, "is_xml_valid");
  static_assert (can_back_up_t<lumex::path>::value, "backup");
  static_assert (can_construct_guard_t<lumex::path>::value, "constructor");
  SUCCEED ();
}

TEST (
    LumexSettingsStdPathForm,
    GivenStdFilesystemPathConvertedToLumexPath_WhenSavedAndLoaded_ThenValueRoundTrips)
{
  std_path_t const directory = "test_settings_std_path_roundtrip";
  std::filesystem::remove_all (directory);
  ASSERT_TRUE (std::filesystem::create_directories (directory));
  std_path_t const file = directory / "settings.ini";

  LumexSettingsINI writer;
  writer.add ("pump", "flow", "1.5");
  bool const saved = writer.save (lumex::path (file));
  LumexSettingsINI reader;
  bool const loaded = reader.load (lumex::path (file));
  std::string const flow = reader.get ("pump", "flow");
  std::filesystem::remove_all (directory);

  EXPECT_TRUE (saved);
  EXPECT_TRUE (loaded);
  EXPECT_EQ (flow, "1.5");
}

#if !defined(_WIN32)
TEST (
    LumexSettingsStdPathForm,
    GivenStdFilesystemPathOnPosix_WhenLoadAndSave_ThenTheStringFormGetsItsText)
{
  recording_settings_t settings;
  std_path_t const file = "dir/file.ini";

  EXPECT_TRUE (settings.load (file));
  EXPECT_TRUE (settings.save (file));

  EXPECT_EQ (settings.last_load, "dir/file.ini");
  EXPECT_EQ (settings.last_save, "dir/file.ini");
}
#endif
