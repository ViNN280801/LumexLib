// lumex/tests/applied/settings/LumexSettingsGuard.cxx11.tests.cpp
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/applied/settings/LumexSettings"
#include "lumex/core/filesystem/LumexFilesystem"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpadded"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage"
#pragma clang diagnostic ignored "-Wunsafe-buffer-usage-in-libc-call"
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#pragma clang diagnostic ignored "-Wnrvo"
#pragma clang diagnostic ignored "-Wheader-hygiene"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wundefined-var-template"
#pragma clang diagnostic ignored "-Wdeprecated-redundant-constexpr-static-def"
#pragma clang diagnostic ignored "-Wvariadic-macro-arguments-omitted"
#pragma clang diagnostic ignored "-Wunused-result"
#pragma clang diagnostic ignored "-Wextra-semi-stmt"
#pragma clang diagnostic ignored "-Wexpansion-to-defined"
#pragma clang diagnostic ignored "-Wexit-time-destructors"
#pragma clang diagnostic ignored "-Wundefined-func-template"
#pragma clang diagnostic ignored "-Wfloat-equal"
#pragma clang diagnostic ignored "-Wglobal-constructors"
#endif

#if defined(__clang__)
#endif

using namespace lumex::applied::settings;
using namespace lumex::applied::settings::guard;
using namespace lumex::core::filesystem::fs;

namespace
{
// Messages of the exceptions FakeLumexSettings throws; the tests look for
// them in the guard's log.
char const *const LOAD_ERROR = "fake load failure";
char const *const SAVE_ERROR = "fake save failure";
char const *const GET_ERROR = "fake get failure";
char const *const ADD_ERROR = "fake add failure";

// Path every FakeLumexSettings-based guard below is constructed with.
char const *const FAKE_FILE = "file.ini";

// Thrown by FakeLumexSettings when an exception that does not derive from
// std::exception is requested.
struct non_std_exception_t
{
};

// Which FakeLumexSettings member throws, and what it throws.
enum class ThrowKind
{
  NONE,
  STD_EXCEPTION,
  NON_STD_EXCEPTION
};

void
throw_if_requested (ThrowKind kind, char const *message)
{
  if (kind == ThrowKind::STD_EXCEPTION)
    throw std::runtime_error (message);
  if (kind == ThrowKind::NON_STD_EXCEPTION)
    throw non_std_exception_t ();
}

// Redirects a standard stream into a string for the lifetime of the object.
class StreamCapture
{
public:
  explicit StreamCapture (std::ostream &stream)
      : m_stream (stream), m_old (stream.rdbuf (m_buffer.rdbuf ()))
  {
  }

  ~StreamCapture () { m_stream.rdbuf (m_old); }

  StreamCapture (StreamCapture const &) = delete;
  StreamCapture &operator= (StreamCapture const &) = delete;

  std::string
  text () const
  {
    return m_buffer.str ();
  }

private:
  std::ostream &m_stream;
  std::ostringstream m_buffer;
  std::streambuf *m_old;
};

// Minimal, fully in-memory ILumexSettings test double. Gives tests precise,
// deterministic control over load()/save() outcomes and call counts,
// independent of LumexSettingsINI's own parsing/validity quirks - used to
// exercise LumexSettingsGuard's own control flow in isolation.
class FakeLumexSettings : public ILumexSettings
{
public:
  bool loadResult = true;
  bool saveResult = true;
  ThrowKind loadThrows = ThrowKind::NONE;
  ThrowKind saveThrows = ThrowKind::NONE;
  ThrowKind getThrows = ThrowKind::NONE;
  ThrowKind addThrows = ThrowKind::NONE;
  int loadCallCount = 0;
  mutable int saveCallCount = 0;
  std::string lastLoadPath;
  mutable std::string lastSavePath;

  bool
  load (std::string const &path) override
  {
    ++loadCallCount;
    lastLoadPath = path;
    throw_if_requested (loadThrows, LOAD_ERROR);
    return loadResult;
  }

  bool
  save (std::string const &path) const override
  {
    ++saveCallCount;
    lastSavePath = path;
    throw_if_requested (saveThrows, SAVE_ERROR);
    return saveResult;
  }

  std::string
  get (std::string const &section, std::string const &key) const override
  {
    throw_if_requested (getThrows, GET_ERROR);
    auto sectionIt = m_values.find (section);
    if (sectionIt == m_values.end ())
      return "";
    auto keyIt = sectionIt->second.find (key);
    if (keyIt == sectionIt->second.end ())
      return "";
    return keyIt->second;
  }

  void
  add (std::string const &section, std::string const &key,
       std::string const &value) override
  {
    throw_if_requested (addThrows, ADD_ERROR);
    m_values[section][key] = value;
  }

  void
  remove (std::string const &section, std::string const &key) override
  {
    auto sectionIt = m_values.find (section);
    if (sectionIt == m_values.end ())
      return;
    sectionIt->second.erase (key);
  }

private:
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      m_values;
};

void
remove_directory_if_exists (lumex::path const &dir)
{
  if (!lumex::core::filesystem::fs::lumex_filesystem::exists (dir))
    return;
  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::remove_all (dir);
  if (!result.success ())
    std::cerr << "Warning: Failed to remove test directory: " << dir
              << std::endl;
}

lumex::path
unique_settings_guard_dir ()
{
  ::testing::TestInfo const *info
      = ::testing::UnitTest::GetInstance ()->current_test_info ();
  std::string dir_name = "test_settings_guard_";
  dir_name += info->test_suite_name ();
  dir_name += "_";
  dir_name += info->name ();
  return lumex::path (dir_name);
}

// filesystem fixture, mirroring LumexSettingsINITest's own setup, for tests
// that need real files on disk (backup(), and end-to-end
// ensureExistsWithDefaults() with LumexSettingsINI).
class LumexSettingsGuardFileTest : public ::testing::Test
{
protected:
  lumex::path _test_dir;
  lumex::path _test_file;

  void
  SetUp () override
  {
    _test_dir = unique_settings_guard_dir ();
    _test_file = _test_dir / "test.ini";

    remove_directory_if_exists (_test_dir);

    auto result
        = lumex::core::filesystem::fs::lumex_filesystem::create_directories (
            _test_dir);
    if (!result.success ())
      std::cerr << "Warning: Failed to create test directory: " << _test_dir
                << std::endl;
  }

  void
  TearDown () override
  {
    remove_directory_if_exists (_test_dir);
  }

  void
  create_test_file (lumex::path const &path, std::string const &content)
  {
    std::ofstream file (path.string ());
    ASSERT_TRUE (file.is_open ())
        << "Failed to create test file: " << path.string ();
    file << content;
    file.close ();
  }

  // True if the test directory contains a file named
  // "<original>.bak.<something>".
  bool
  has_backup_of (lumex::path const &original)
  {
    auto listing
        = lumex::core::filesystem::fs::lumex_filesystem::directory_paths (
            _test_dir);
    if (!listing.success ())
      return false;

    std::string const prefix = original.filename ().string () + ".bak.";
    for (auto const &entry : listing.value ())
      if (entry.filename ().string ().rfind (prefix, 0) == 0)
        return true;
    return false;
  }
};

TEST (LumexSettingsGuardCleanup,
      RemoveDirectoryIfExistsDeletesCreatedDirectory)
{
  lumex::path const dir ("test_settings_guard_cleanup_probe");
  remove_directory_if_exists (dir);
  ASSERT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::create_directories (dir)
          .success ());
  ASSERT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::exists (dir));

  std::ofstream file ((dir / "test.ini").string ());
  ASSERT_TRUE (file.is_open ());
  file << "[section]\nkey=value\n";
  file.close ();

  remove_directory_if_exists (dir);
  EXPECT_FALSE (lumex::core::filesystem::fs::lumex_filesystem::exists (dir));
}
} // namespace

// --- Construction / accessors ----------------------------------------------

TEST (LumexSettingsGuardTest,
      GivenSettingsAndFilename_WhenConstructed_ThenAccessorsReturnThem)
{
  auto settings = std::make_shared<FakeLumexSettings> ();
  LumexSettingsGuard guard (settings, "some/path.ini");

  EXPECT_EQ (guard.settings (), settings);
  EXPECT_EQ (guard.filename (), "some/path.ini");
}

// --- Null-settings defensiveness --------------------------------------------

TEST (
    LumexSettingsGuardTest,
    GivenNullSettings_WhenEnsureExistsWithDefaults_ThenReturnsFalseWithoutCallingCreateDefault)
{
  LumexSettingsGuard guard (nullptr, "whatever.ini");
  bool createCalled = false;

  EXPECT_FALSE (guard.ensureExistsWithDefaults (
      [&] ()
        {
          createCalled = true;
          return true;
        }));
  EXPECT_FALSE (createCalled);
}

TEST (LumexSettingsGuardTest,
      GivenNullSettings_WhenRepairIfCorrupted_ThenReturnsFalse)
{
  LumexSettingsGuard guard (nullptr, "whatever.ini");
  EXPECT_FALSE (guard.repairIfCorrupted ([] () { return true; }));
}

TEST (LumexSettingsGuardTest,
      GivenNullSettings_WhenEnsureKeysWithDefaults_ThenReturnsFalse)
{
  LumexSettingsGuard guard (nullptr, "whatever.ini");
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key", "default",
                                                  nullptr } };
  EXPECT_FALSE (guard.ensureKeysWithDefaults (specs));
}

// --- ensureExistsWithDefaults() control flow (FakeLumexSettings) -----------

TEST (
    LumexSettingsGuardTest,
    GivenLoadSucceeds_WhenEnsureExistsWithDefaults_ThenCreateDefaultNotCalledAndReturnsTrue)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = true;
  LumexSettingsGuard guard (fake, "file.ini");
  bool createCalled = false;

  EXPECT_TRUE (guard.ensureExistsWithDefaults (
      [&] ()
        {
          createCalled = true;
          return true;
        }));
  EXPECT_FALSE (createCalled);
  EXPECT_EQ (fake->loadCallCount, 1);
}

TEST (
    LumexSettingsGuardTest,
    GivenLoadFailsThenCreateDefaultSucceeds_WhenEnsureExistsWithDefaults_ThenRetriesLoadAndReturnsTrue)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = false;
  LumexSettingsGuard guard (fake, "file.ini");
  int createCallCount = 0;

  bool const result = guard.ensureExistsWithDefaults (
      [&] ()
        {
          ++createCallCount;
          fake->loadResult
              = true; // simulate defaults now being on disk / loadable
          return true;
        });

  EXPECT_TRUE (result);
  EXPECT_EQ (createCallCount, 1);
  EXPECT_EQ (fake->loadCallCount,
             2); // initial failing attempt + retry after repair
}

TEST (
    LumexSettingsGuardTest,
    GivenCreateDefaultFails_WhenEnsureExistsWithDefaults_ThenReturnsFalseWithoutRetryingLoad)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = false;
  LumexSettingsGuard guard (fake, "file.ini");

  EXPECT_FALSE (guard.ensureExistsWithDefaults ([] () { return false; }));
  EXPECT_EQ (fake->loadCallCount,
             1); // no point retrying load() if repair itself failed
}

TEST (
    LumexSettingsGuardTest,
    GivenEmptyCreateDefaultFunctor_WhenEnsureExistsWithDefaults_ThenReturnsFalse)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = false;
  LumexSettingsGuard guard (fake, "file.ini");

  LumexSettingsCreateFn empty; // default-constructed std::function is falsy
  EXPECT_FALSE (guard.ensureExistsWithDefaults (empty));
}

TEST (
    LumexSettingsGuardTest,
    GivenCreateDefaultThrows_WhenEnsureExistsWithDefaults_ThenReturnsFalseWithoutCrashing)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = false;
  LumexSettingsGuard guard (fake, "file.ini");

  EXPECT_FALSE (guard.ensureExistsWithDefaults (
      [] () -> bool { throw std::runtime_error ("boom"); }));
}

TEST (
    LumexSettingsGuardTest,
    GivenLoadStillFailsAfterCreateDefault_WhenEnsureExistsWithDefaults_ThenReturnsFalse)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = false; // never recovers
  LumexSettingsGuard guard (fake, "file.ini");

  EXPECT_FALSE (guard.ensureExistsWithDefaults ([] () { return true; }));
  EXPECT_EQ (fake->loadCallCount, 2);
}

// --- repairIfCorrupted() mirrors ensureExistsWithDefaults's control flow ---

TEST (
    LumexSettingsGuardTest,
    GivenLoadSucceeds_WhenRepairIfCorrupted_ThenCreateDefaultNotCalledAndReturnsTrue)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = true;
  LumexSettingsGuard guard (fake, "file.ini");
  bool createCalled = false;

  EXPECT_TRUE (guard.repairIfCorrupted (
      [&] ()
        {
          createCalled = true;
          return true;
        }));
  EXPECT_FALSE (createCalled);
}

TEST (
    LumexSettingsGuardTest,
    GivenLoadFailsThenCreateDefaultSucceeds_WhenRepairIfCorrupted_ThenReturnsTrue)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = false;
  LumexSettingsGuard guard (fake, "file.ini");

  EXPECT_TRUE (guard.repairIfCorrupted (
      [&] ()
        {
          fake->loadResult = true;
          return true;
        }));
}

// --- backup()
// ----------------------------------------------------------------

TEST_F (LumexSettingsGuardFileTest,
        GivenNonExistentFile_WhenBackup_ThenReturnsFalse)
{
  EXPECT_FALSE (LumexSettingsGuard::backup (
      (_test_dir / "does_not_exist.ini").string ()));
}

TEST_F (LumexSettingsGuardFileTest,
        GivenDirectoryPath_WhenBackup_ThenReturnsFalse)
{
  EXPECT_FALSE (LumexSettingsGuard::backup (_test_dir.string ()));
}

TEST_F (LumexSettingsGuardFileTest,
        GivenExistingFile_WhenBackup_ThenCreatesTimestampedCopyWithSameContent)
{
  create_test_file (_test_file, "[section]\nkey=value\n");

  EXPECT_TRUE (LumexSettingsGuard::backup (_test_file.string ()));
  EXPECT_TRUE (has_backup_of (_test_file));

  // The original file itself must be left untouched by backup().
  std::ifstream original (_test_file.string ());
  std::string const content ((std::istreambuf_iterator<char> (original)),
                             std::istreambuf_iterator<char> ());
  original.close ();
  EXPECT_EQ (content, "[section]\nkey=value\n");
}

// --- ensureExistsWithDefaults() end-to-end with a real LumexSettingsINI ----

TEST_F (
    LumexSettingsGuardFileTest,
    GivenCorruptIniFile_WhenEnsureExistsWithDefaults_ThenBacksUpRecreatesAndLoads)
{
  create_test_file (
      _test_file,
      "[section\nkey=value\n"); // malformed section header => load() fails

  auto ini = std::make_shared<LumexSettingsINI> ();
  LumexSettingsGuard guard (ini, _test_file.string ());

  lumex::path const targetFile = _test_file;
  bool const result = guard.ensureExistsWithDefaults (
      [targetFile] ()
        {
          std::ofstream out (targetFile.string ());
          out << "[section]\nkey=value\n";
          return out.good ();
        });

  EXPECT_TRUE (result);
  EXPECT_EQ (ini->get ("section", "key"), "value");
  EXPECT_TRUE (has_backup_of (_test_file));
}

TEST_F (
    LumexSettingsGuardFileTest,
    GivenMissingIniFile_WhenEnsureExistsWithDefaults_ThenCreatesAndLoadsWithoutBackup)
{
  // _test_file was never created by this test.
  auto ini = std::make_shared<LumexSettingsINI> ();
  LumexSettingsGuard guard (ini, _test_file.string ());

  lumex::path const targetFile = _test_file;
  bool const result = guard.ensureExistsWithDefaults (
      [targetFile] ()
        {
          std::ofstream out (targetFile.string ());
          out << "[section]\nkey=value\n";
          return out.good ();
        });

  EXPECT_TRUE (result);
  EXPECT_EQ (ini->get ("section", "key"), "value");
  EXPECT_FALSE (has_backup_of (_test_file)); // nothing existed to back up
}

// --- ensureKeysWithDefaults() control flow (FakeLumexSettings) -------------

TEST (LumexSettingsGuardTest,
      GivenMissingKey_WhenEnsureKeysWithDefaults_ThenSetsDefaultAndReturnsTrue)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  LumexSettingsGuard guard (fake, "file.ini");
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key",
                                                  "default_value", nullptr } };

  EXPECT_TRUE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->get ("section", "key"), "default_value");
  EXPECT_EQ (fake->saveCallCount, 1);
}

TEST (
    LumexSettingsGuardTest,
    GivenEmptyValueNoValidator_WhenEnsureKeysWithDefaults_ThenReplacedWithDefault)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->add ("section", "key", "");
  LumexSettingsGuard guard (fake, "file.ini");
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key",
                                                  "default_value", nullptr } };

  EXPECT_TRUE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->get ("section", "key"), "default_value");
}

TEST (
    LumexSettingsGuardTest,
    GivenNonEmptyValueNoValidator_WhenEnsureKeysWithDefaults_ThenLeftUnchangedAndReturnsFalse)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->add ("section", "key", "existing_value");
  LumexSettingsGuard guard (fake, "file.ini");
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key",
                                                  "default_value", nullptr } };

  EXPECT_FALSE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->get ("section", "key"), "existing_value");
  EXPECT_EQ (fake->saveCallCount, 0);
}

TEST (
    LumexSettingsGuardTest,
    GivenValidatorRejectsValue_WhenEnsureKeysWithDefaults_ThenReplacedWithDefault)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->add ("section", "key", "not_a_number");
  LumexSettingsGuard guard (fake, "file.ini");
  std::vector<lumex_settings_key_spec_t> specs{
    { "section", "key", "42",
      [] (std::string const &value)
        {
          return !value.empty ()
                 && value.find_first_not_of ("0123456789")
                        == std::string::npos;
        } }
  };

  EXPECT_TRUE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->get ("section", "key"), "42");
}

TEST (LumexSettingsGuardTest,
      GivenValidatorAcceptsValue_WhenEnsureKeysWithDefaults_ThenLeftUnchanged)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->add ("section", "key", "123");
  LumexSettingsGuard guard (fake, "file.ini");
  std::vector<lumex_settings_key_spec_t> specs{
    { "section", "key", "42",
      [] (std::string const &value)
        {
          return !value.empty ()
                 && value.find_first_not_of ("0123456789")
                        == std::string::npos;
        } }
  };

  EXPECT_FALSE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->get ("section", "key"), "123");
}

TEST (
    LumexSettingsGuardTest,
    GivenValidatorThrows_WhenEnsureKeysWithDefaults_ThenTreatedAsInvalidAndReplaced)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->add ("section", "key", "value");
  LumexSettingsGuard guard (fake, "file.ini");
  std::vector<lumex_settings_key_spec_t> specs{
    { "section", "key", "default_value",
      [] (std::string const &) -> bool { throw std::runtime_error ("boom"); } }
  };

  EXPECT_TRUE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->get ("section", "key"), "default_value");
}

TEST (
    LumexSettingsGuardTest,
    GivenSaveFails_WhenEnsureKeysWithDefaults_ThenReturnsFalseEvenThoughKeyWasChanged)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->saveResult = false;
  LumexSettingsGuard guard (fake, "file.ini");
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key",
                                                  "default_value", nullptr } };

  EXPECT_FALSE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->get ("section", "key"),
             "default_value"); // still applied in memory
}

TEST (
    LumexSettingsGuardTest,
    GivenMultipleSpecs_WhenEnsureKeysWithDefaults_ThenAllAppliedWithASingleSave)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->add ("section", "good", "keep_me");
  LumexSettingsGuard guard (fake, "file.ini");
  std::vector<lumex_settings_key_spec_t> specs{
    { "section", "good", "unused", nullptr },
    { "section", "empty", "filled", nullptr },
    { "section2", "missing", "created", nullptr },
  };

  EXPECT_TRUE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->get ("section", "good"), "keep_me");
  EXPECT_EQ (fake->get ("section", "empty"), "filled");
  EXPECT_EQ (fake->get ("section2", "missing"), "created");
  EXPECT_EQ (fake->saveCallCount, 1);
}

// --- ensureKeysWithDefaults() end-to-end with a real LumexSettingsINI ------

TEST_F (
    LumexSettingsGuardFileTest,
    GivenIniLoadedFromDisk_WhenEnsureKeysWithDefaultsChanges_ThenPersistsToFile)
{
  create_test_file (_test_file, "[section]\nkey=\n");

  auto ini = std::make_shared<LumexSettingsINI> ();
  ASSERT_TRUE (ini->load (_test_file));

  LumexSettingsGuard guard (ini, _test_file.string ());
  std::vector<lumex_settings_key_spec_t> specs{
    { "section", "key", "restored_default", nullptr }
  };

  EXPECT_TRUE (guard.ensureKeysWithDefaults (specs));

  LumexSettingsINI reloaded;
  ASSERT_TRUE (reloaded.load (_test_file));
  EXPECT_EQ (reloaded.get ("section", "key"), "restored_default");
}

// --- Exceptions from the guarded settings object ----------------------------
//
// Every member of the guard is noexcept. An exception from the guarded
// ILumexSettings must end the call with false and a log line naming the
// exception, never std::terminate().

TEST (
    LumexSettingsGuardTest,
    GivenSaveThrows_WhenEnsureKeysWithDefaults_ThenReturnsFalseAndLogsWarning)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->saveThrows = ThrowKind::STD_EXCEPTION;
  LumexSettingsGuard guard (fake, FAKE_FILE);
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key",
                                                  "default_value", nullptr } };

  bool result = true;
  std::string log;
  {
    StreamCapture capture (std::clog);
    result = guard.ensureKeysWithDefaults (specs);
    log = capture.text ();
  }

  EXPECT_FALSE (result);
  EXPECT_EQ (fake->saveCallCount, 1);
  EXPECT_NE (log.find (SAVE_ERROR), std::string::npos) << log;
  EXPECT_NE (log.find (FAKE_FILE), std::string::npos) << log;
}

TEST (
    LumexSettingsGuardTest,
    GivenSaveThrowsNonStdException_WhenEnsureKeysWithDefaults_ThenReturnsFalse)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->saveThrows = ThrowKind::NON_STD_EXCEPTION;
  LumexSettingsGuard guard (fake, FAKE_FILE);
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key",
                                                  "default_value", nullptr } };

  bool result = true;
  std::string log;
  {
    StreamCapture capture (std::clog);
    result = guard.ensureKeysWithDefaults (specs);
    log = capture.text ();
  }

  EXPECT_FALSE (result);
  EXPECT_NE (log.find (FAKE_FILE), std::string::npos) << log;
}

TEST (LumexSettingsGuardTest,
      GivenGetThrows_WhenEnsureKeysWithDefaults_ThenReturnsFalseWithoutSaving)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->getThrows = ThrowKind::STD_EXCEPTION;
  LumexSettingsGuard guard (fake, FAKE_FILE);
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key",
                                                  "default_value", nullptr } };

  bool result = true;
  std::string log;
  {
    StreamCapture capture (std::clog);
    result = guard.ensureKeysWithDefaults (specs);
    log = capture.text ();
  }

  EXPECT_FALSE (result);
  EXPECT_EQ (fake->saveCallCount, 0);
  EXPECT_NE (log.find (GET_ERROR), std::string::npos) << log;
}

TEST (
    LumexSettingsGuardTest,
    GivenAddThrowsOnSecondKey_WhenEnsureKeysWithDefaults_ThenReturnsFalseKeepsFirstKeyAndDoesNotSave)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  LumexSettingsGuard guard (fake, FAKE_FILE);
  std::vector<lumex_settings_key_spec_t> specs{
    { "section", "first", "first_default", nullptr },
    { "section", "second", "second_default",
      [fake] (std::string const &)
        {
          // Reached after the first default was added: every later add ()
          // throws.
          fake->addThrows = ThrowKind::STD_EXCEPTION;
          return false;
        } },
  };

  bool result = true;
  std::string log;
  {
    StreamCapture capture (std::clog);
    result = guard.ensureKeysWithDefaults (specs);
    log = capture.text ();
  }

  EXPECT_FALSE (result);
  EXPECT_EQ (fake->saveCallCount, 0);
  EXPECT_NE (log.find (ADD_ERROR), std::string::npos) << log;
  fake->addThrows = ThrowKind::NONE;
  EXPECT_EQ (fake->get ("section", "first"), "first_default");
  EXPECT_EQ (fake->get ("section", "second"), "");
}

TEST (
    LumexSettingsGuardTest,
    GivenSaveThrewOnce_WhenEnsureKeysWithDefaultsAgain_ThenGuardIsUsableAndSaves)
{
  // The guard's mutex must be released when the exception ends the call.
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->saveThrows = ThrowKind::STD_EXCEPTION;
  LumexSettingsGuard guard (fake, FAKE_FILE);
  std::vector<lumex_settings_key_spec_t> specs{ { "section", "key",
                                                  "default_value", nullptr } };
  {
    StreamCapture capture (std::clog);
    EXPECT_FALSE (guard.ensureKeysWithDefaults (specs));
  }

  fake->saveThrows = ThrowKind::NONE;
  fake->remove ("section", "key");
  EXPECT_TRUE (guard.ensureKeysWithDefaults (specs));
  EXPECT_EQ (fake->saveCallCount, 2);
  EXPECT_EQ (fake->lastSavePath, FAKE_FILE);
}

TEST (
    LumexSettingsGuardTest,
    GivenLoadThrows_WhenEnsureExistsWithDefaults_ThenReturnsFalseLogsErrorAndDoesNotRegenerate)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadThrows = ThrowKind::STD_EXCEPTION;
  LumexSettingsGuard guard (fake, FAKE_FILE);
  bool createCalled = false;

  bool result = true;
  std::string log;
  {
    StreamCapture capture (std::cerr);
    result = guard.ensureExistsWithDefaults (
        [&] ()
          {
            createCalled = true;
            return true;
          });
    log = capture.text ();
  }

  EXPECT_FALSE (result);
  EXPECT_FALSE (createCalled);
  EXPECT_EQ (fake->loadCallCount, 1);
  EXPECT_NE (log.find (LOAD_ERROR), std::string::npos) << log;
  EXPECT_NE (log.find (FAKE_FILE), std::string::npos) << log;
}

TEST (
    LumexSettingsGuardTest,
    GivenLoadThrowsAfterCreateDefault_WhenEnsureExistsWithDefaults_ThenReturnsFalse)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadResult = false;
  LumexSettingsGuard guard (fake, FAKE_FILE);

  bool result = true;
  std::string log;
  {
    StreamCapture capture (std::cerr);
    result = guard.ensureExistsWithDefaults (
        [&] ()
          {
            fake->loadThrows = ThrowKind::NON_STD_EXCEPTION;
            return true;
          });
    log = capture.text ();
  }

  EXPECT_FALSE (result);
  EXPECT_EQ (fake->loadCallCount, 2);
  EXPECT_NE (log.find (FAKE_FILE), std::string::npos) << log;
}

TEST (LumexSettingsGuardTest,
      GivenLoadThrows_WhenRepairIfCorrupted_ThenReturnsFalseAndLogsWarning)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadThrows = ThrowKind::STD_EXCEPTION;
  LumexSettingsGuard guard (fake, FAKE_FILE);

  bool result = true;
  std::string log;
  {
    StreamCapture capture (std::clog);
    result = guard.repairIfCorrupted ([] () { return true; });
    log = capture.text ();
  }

  EXPECT_FALSE (result);
  EXPECT_NE (log.find (LOAD_ERROR), std::string::npos) << log;
}

TEST (LumexSettingsGuardTest,
      GivenLoadThrewOnce_WhenEnsureExistsWithDefaultsAgain_ThenGuardIsUsable)
{
  auto fake = std::make_shared<FakeLumexSettings> ();
  fake->loadThrows = ThrowKind::STD_EXCEPTION;
  LumexSettingsGuard guard (fake, FAKE_FILE);
  {
    StreamCapture capture (std::cerr);
    EXPECT_FALSE (guard.ensureExistsWithDefaults ([] () { return true; }));
  }

  fake->loadThrows = ThrowKind::NONE;
  EXPECT_TRUE (guard.ensureExistsWithDefaults ([] () { return true; }));
  EXPECT_EQ (fake->loadCallCount, 2);
}

#if defined(LUMEX_SETTINGS_WITH_JSON)
TEST_F (
    LumexSettingsGuardFileTest,
    GivenJsonDefaultWithInvalidUtf8_WhenEnsureKeysWithDefaults_ThenReturnsFalseInsteadOfTerminating)
{
  // LumexSettingsJSON::save () throws nlohmann's type_error 316 for a value
  // that is not valid UTF-8.
  auto json
      = std::make_shared<lumex::applied::settings::json::LumexSettingsJSON> ();
  std::string const jsonFile = (_test_dir / "test.json").string ();
  LumexSettingsGuard guard (json, jsonFile);
  std::vector<lumex_settings_key_spec_t> specs{
    { "section", "key", std::string ("\xC3\x28"), nullptr }
  };

  bool result = true;
  std::string log;
  {
    StreamCapture capture (std::clog);
    result = guard.ensureKeysWithDefaults (specs);
    log = capture.text ();
  }

  EXPECT_FALSE (result);
  EXPECT_NE (log.find (jsonFile), std::string::npos) << log;
}
#endif
