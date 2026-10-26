// lumex/tests/core/interprocess/lock/LumexInterprocessLock.cxx11.tests.cpp
//
// interprocess_lock: taking, refusing, releasing and moving the lock, the
// payload, the lock file and its directories, the failures (values, never
// exceptions), and a process that ends without releasing. Another lock object
// of this process counts as another owner (flock and LockFileEx lock the open
// file, not the process), so most cases need one process only; the end of a
// process is a death test.
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/interprocess/LumexInterprocess"
#include "lumex/core/utility/process/LumexProcess.hpp"

namespace lk = lumex::core::interprocess::lock;
using lumex::core::filesystem::fs::lumex_filesystem;
using lumex::core::filesystem::fs::path;

namespace
{
// "Привет" (Russian) in UTF-8.
char const *const privet_utf8
    = "\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";

class LumexInterprocessLockTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    // One directory per case and process: gtest_discover_tests runs every
    // case in its own process, and the suites of all standards may run the
    // same case at the same time.
    m_base = path (
        std::string ("lumex_interprocess_")
        + ::testing::UnitTest::GetInstance ()->current_test_info ()->name ()
        + "_"
        + std::to_string (lumex::core::utility::process::get_current_pid ()));
    lumex_filesystem::remove_all (m_base);
    ASSERT_TRUE (lumex_filesystem::create_directories (m_base).success ());
    m_file = m_base / "device_a.lock";
  }

  void
  TearDown () override
  {
    lumex_filesystem::remove_all (m_base);
  }

  path m_base;
  path m_file;
};

#if defined(_WIN32)
int const busy_code = ERROR_LOCK_VIOLATION;
#else
int const busy_code = EWOULDBLOCK;
#endif

// The variable that hands the lock file to the helper process on Windows.
char const *const hold_file_variable = "LUMEX_INTERPROCESS_TEST_HOLD_FILE";
// The payload of the helper: the parent waits for it before it checks.
char const *const child_payload = "held by the child";

// Takes the lock of `file` and keeps it until the process is killed (at most
// 60 s, should the parent be gone). Runs in the second process only.
void
hold_until_killed (path const &file)
{
  auto lock = lk::interprocess_lock::try_acquire (file, child_payload);
  if (!lock)
    std::_Exit (1);
  std::this_thread::sleep_for (std::chrono::seconds (60));
  std::_Exit (0);
}

// A second process that holds the lock of a file, ended by force (no
// destructor, no unlock: the way a crash or the task manager ends it). On
// Windows it is this test binary again, running only
// DISABLED_HelperOfAnotherProcess; on POSIX a fork, which takes the lock
// through its own open of the file.
class lock_holding_process
{
public:
  lock_holding_process () = default;
  lock_holding_process (lock_holding_process const &) = delete;
  lock_holding_process &operator= (lock_holding_process const &) = delete;

  ~lock_holding_process () { kill (); }

  bool
  start (path const &file)
  {
#if defined(_WIN32)
    std::wstring self (MAX_PATH, L'\0');
    DWORD const length = GetModuleFileNameW (
        nullptr, &self[0], static_cast<DWORD> (self.size ()));
    if (length == 0 || length >= self.size ())
      return false;
    self.resize (length);
    std::wstring command
        = L"\"" + self
          + L"\" --gtest_filter=LumexInterprocessLockTest."
            L"DISABLED_HelperOfAnotherProcess --gtest_also_run_disabled_tests";
    SetEnvironmentVariableW (
        lumex_filesystem::to_wide_string (hold_file_variable).c_str (),
        file.wstring ().c_str ());
    STARTUPINFOW startup = {};
    startup.cb = sizeof (startup);
    PROCESS_INFORMATION process = {};
    BOOL const started = CreateProcessW (self.c_str (), &command[0], nullptr,
                                         nullptr, FALSE, CREATE_NO_WINDOW,
                                         nullptr, nullptr, &startup, &process);
    SetEnvironmentVariableW (
        lumex_filesystem::to_wide_string (hold_file_variable).c_str (),
        nullptr);
    if (started == 0)
      return false;
    CloseHandle (process.hThread);
    m_process = process.hProcess;
    return true;
#else
    pid_t const child = fork ();
    if (child < 0)
      return false;
    if (child == 0)
      hold_until_killed (file);
    m_process = child;
    return true;
#endif
  }

  bool
  running () const
  {
#if defined(_WIN32)
    return m_process != nullptr
           && WaitForSingleObject (m_process, 0) == WAIT_TIMEOUT;
#else
    return m_process > 0 && waitpid (m_process, nullptr, WNOHANG) == 0;
#endif
  }

  void
  kill ()
  {
#if defined(_WIN32)
    if (m_process == nullptr)
      return;
    TerminateProcess (m_process, 9);
    WaitForSingleObject (m_process, INFINITE);
    CloseHandle (m_process);
    m_process = nullptr;
#else
    if (m_process <= 0)
      return;
    ::kill (m_process, SIGKILL);
    waitpid (m_process, nullptr, 0);
    m_process = -1;
#endif
  }

private:
#if defined(_WIN32)
  HANDLE m_process = nullptr;
#else
  pid_t m_process = -1;
#endif
};

// Waits until the payload of `file` is `text`: the other process holds the
// lock then. False after 20 s.
bool
wait_for_payload (path const &file, std::string const &text)
{
  for (int attempt = 0; attempt < 200; ++attempt)
    {
      auto const payload = lk::interprocess_lock::read_payload (file);
      if (payload && payload.value () == text)
        return true;
      std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }
  return false;
}

// Takes the lock, retrying for up to 5 s: on Windows the system may release
// the lock of an ended process a little later.
bool
acquire_within_seconds (path const &file)
{
  for (int attempt = 0; attempt < 50; ++attempt)
    {
      if (lk::interprocess_lock::try_acquire (file).has_value ())
        return true;
      std::this_thread::sleep_for (std::chrono::milliseconds (100));
    }
  return false;
}
} // namespace

TEST_F (LumexInterprocessLockTest,
        GivenFreeFile_WhenTryAcquire_ThenItOwnsTheLockAndWritesThePayload)
{
  auto lock = lk::interprocess_lock::try_acquire (m_file, "{\"pid\":1}");

  ASSERT_TRUE (lock.has_value ())
      << to_string (lock.error ().kind) << " " << lock.error ().system_code;
  EXPECT_TRUE (lock.value ().owns_lock ());
  auto const payload = lk::interprocess_lock::read_payload (m_file);
  ASSERT_TRUE (payload.has_value ());
  EXPECT_EQ (payload.value (), "{\"pid\":1}");
}

TEST_F (LumexInterprocessLockTest,
        GivenHeldLock_WhenAnotherObjectTriesTheSameFile_ThenBusy)
{
  auto first = lk::interprocess_lock::try_acquire (m_file, "first");
  ASSERT_TRUE (first.has_value ());

  auto const second = lk::interprocess_lock::try_acquire (m_file, "second");

  ASSERT_FALSE (second.has_value ());
  EXPECT_EQ (second.error ().kind, lk::lock_error::busy);
  EXPECT_EQ (second.error ().system_code, busy_code);
  // The refused attempt does not touch the payload of the holder.
  EXPECT_EQ (lk::interprocess_lock::read_payload (m_file).value (), "first");
}

TEST_F (LumexInterprocessLockTest,
        GivenHeldLocks_WhenDifferentFiles_ThenBothAreTaken)
{
  auto first = lk::interprocess_lock::try_acquire (m_base / "device_a.lock");
  auto second = lk::interprocess_lock::try_acquire (m_base / "device_b.lock");

  EXPECT_TRUE (first.has_value ());
  EXPECT_TRUE (second.has_value ());
}

TEST_F (LumexInterprocessLockTest,
        GivenHeldLock_WhenReleased_ThenItCanBeTakenAgain)
{
  auto first = lk::interprocess_lock::try_acquire (m_file);
  ASSERT_TRUE (first.has_value ());

  first.value ().release ();
  EXPECT_FALSE (first.value ().owns_lock ());
  // A second release does nothing.
  first.value ().release ();

  EXPECT_TRUE (lk::interprocess_lock::try_acquire (m_file).has_value ());
}

TEST_F (LumexInterprocessLockTest,
        GivenHeldLock_WhenDestroyed_ThenItCanBeTakenAgain)
{
  {
    auto first = lk::interprocess_lock::try_acquire (m_file);
    ASSERT_TRUE (first.has_value ());
  }

  EXPECT_TRUE (lk::interprocess_lock::try_acquire (m_file).has_value ());
}

TEST_F (LumexInterprocessLockTest,
        GivenHeldLock_WhenMoveConstructed_ThenTheNewObjectHoldsIt)
{
  auto acquired = lk::interprocess_lock::try_acquire (m_file);
  ASSERT_TRUE (acquired.has_value ());

  lk::interprocess_lock moved (std::move (acquired.value ()));

  EXPECT_TRUE (moved.owns_lock ());
  EXPECT_FALSE (acquired.value ().owns_lock ());
  // The moved-from object releases nothing when it goes.
  acquired.value ().release ();
  EXPECT_FALSE (lk::interprocess_lock::try_acquire (m_file).has_value ());
}

TEST_F (LumexInterprocessLockTest,
        GivenTwoLocks_WhenMoveAssigned_ThenTheOldLockOfTheTargetIsReleased)
{
  path const other = m_base / "device_b.lock";
  auto target = lk::interprocess_lock::try_acquire (m_file);
  auto source = lk::interprocess_lock::try_acquire (other);
  ASSERT_TRUE (target.has_value ());
  ASSERT_TRUE (source.has_value ());

  target.value () = std::move (source.value ());

  EXPECT_TRUE (target.value ().owns_lock ());
  EXPECT_FALSE (source.value ().owns_lock ());
  EXPECT_TRUE (lk::interprocess_lock::try_acquire (m_file).has_value ());
  EXPECT_FALSE (lk::interprocess_lock::try_acquire (other).has_value ());
}

TEST_F (LumexInterprocessLockTest,
        GivenReleasedLock_WhenTheFileIsChecked_ThenItStaysWithItsPayload)
{
  {
    auto lock = lk::interprocess_lock::try_acquire (m_file, "owner");
    ASSERT_TRUE (lock.has_value ());
  }

  EXPECT_TRUE (lumex_filesystem::is_regular_file (m_file));
  EXPECT_EQ (lk::interprocess_lock::read_payload (m_file).value (), "owner");
}

TEST_F (LumexInterprocessLockTest,
        GivenLongerOldPayload_WhenTakenWithAShortOne_ThenOnlyTheShortOneIsLeft)
{
  {
    auto lock
        = lk::interprocess_lock::try_acquire (m_file, "a much longer payload");
    ASSERT_TRUE (lock.has_value ());
  }

  auto lock = lk::interprocess_lock::try_acquire (m_file, "x");

  ASSERT_TRUE (lock.has_value ());
  EXPECT_EQ (lk::interprocess_lock::read_payload (m_file).value (), "x");
}

TEST_F (LumexInterprocessLockTest,
        GivenEmptyPayload_WhenTaken_ThenTheFileIsEmpty)
{
  {
    auto lock = lk::interprocess_lock::try_acquire (m_file, "old");
    ASSERT_TRUE (lock.has_value ());
  }

  auto lock = lk::interprocess_lock::try_acquire (m_file);

  ASSERT_TRUE (lock.has_value ());
  EXPECT_EQ (lk::interprocess_lock::read_payload (m_file).value (), "");
}

TEST_F (LumexInterprocessLockTest,
        GivenMissingDirectories_WhenTryAcquire_ThenTheyAreCreated)
{
  path const nested = m_base / "a" / "b" / "c" / "device_a.lock";

  auto lock = lk::interprocess_lock::try_acquire (nested, "p");

  ASSERT_TRUE (lock.has_value ());
  EXPECT_TRUE (lumex_filesystem::is_directory (m_base / "a" / "b" / "c"));
  EXPECT_EQ (lk::interprocess_lock::read_payload (nested).value (), "p");
}

TEST_F (LumexInterprocessLockTest,
        GivenCyrillicDirectoryAndName_WhenTryAcquire_ThenTheLockWorks)
{
  path const file
      = m_base / privet_utf8 / (std::string (privet_utf8) + ".lock");

  auto first = lk::interprocess_lock::try_acquire (file, privet_utf8);
  auto const second = lk::interprocess_lock::try_acquire (file);

  ASSERT_TRUE (first.has_value ());
  ASSERT_FALSE (second.has_value ());
  EXPECT_EQ (second.error ().kind, lk::lock_error::busy);
  EXPECT_EQ (lk::interprocess_lock::read_payload (file).value (),
             std::string (privet_utf8));
}

TEST_F (LumexInterprocessLockTest,
        GivenMissingFile_WhenReadPayload_ThenIoErrorWithTheSystemCode)
{
  auto const payload
      = lk::interprocess_lock::read_payload (m_base / "missing.lock");

  ASSERT_FALSE (payload.has_value ());
  EXPECT_EQ (payload.error ().kind, lk::lock_error::io_error);
  EXPECT_NE (payload.error ().system_code, 0);
}

TEST_F (
    LumexInterprocessLockTest,
    GivenDirectoryInPlaceOfTheFile_WhenTryAcquire_ThenAFailureNotAnException)
{
  ASSERT_TRUE (lumex_filesystem::create_directories (m_file).success ());

  lk::interprocess_lock::acquire_result_t lock
      = lk::interprocess_lock::try_acquire (m_file);

  ASSERT_FALSE (lock.has_value ());
  // The same kind on both systems, as the README table says: Windows answers
  // ERROR_ACCESS_DENIED for a directory, POSIX EISDIR.
  EXPECT_EQ (lock.error ().kind, lk::lock_error::io_error);
  EXPECT_NE (lock.error ().system_code, 0);
}

TEST_F (
    LumexInterprocessLockTest,
    GivenFileInPlaceOfTheDirectory_WhenTryAcquire_ThenAFailureNotAnException)
{
  path const blocker = m_base / "not_a_directory";
  {
    auto lock = lk::interprocess_lock::try_acquire (blocker);
    ASSERT_TRUE (lock.has_value ());
  }

  auto const lock
      = lk::interprocess_lock::try_acquire (blocker / "device_a.lock");

  ASSERT_FALSE (lock.has_value ());
  EXPECT_NE (lock.error ().kind, lk::lock_error::busy);
  EXPECT_NE (lock.error ().system_code, 0);
}

namespace
{
// The handles (Windows) or descriptors (Linux, through /proc/self/fd) the
// process holds now; -1 when the system does not tell.
long
open_handle_count ()
{
#if defined(_WIN32)
  DWORD count = 0;
  if (GetProcessHandleCount (GetCurrentProcess (), &count) == 0)
    return -1;
  return static_cast<long> (count);
#elif defined(__linux__)
  DIR *const directory = opendir ("/proc/self/fd");
  if (directory == nullptr)
    return -1;
  long count = 0;
  while (readdir (directory) != nullptr)
    ++count;
  closedir (directory);
  return count;
#else
  return -1;
#endif
}

std::string
bad_ab ()
{
  return std::string ("a\xFF") + "b";
}
} // namespace

// The leak a mutant leaves unseen otherwise: the handle (descriptor) of a lock
// that is released, of a try_acquire that finds the file busy, and of a
// read_payload stays open, and no other case notices.
TEST_F (LumexInterprocessLockTest,
        GivenManyAcquireAndReleaseCycles_WhenCounted_ThenNoHandleIsLeft)
{
  // One round first: whatever the system opens once per process is open
  // before counting.
  {
    auto warm_up = lk::interprocess_lock::try_acquire (m_file, "p");
    ASSERT_TRUE (warm_up.has_value ());
    lk::interprocess_lock::read_payload (m_file);
  }
  long const before = open_handle_count ();
  if (before < 0)
    GTEST_SKIP () << "the system does not report open handles";

  for (int i = 0; i < 100; ++i)
    {
      auto lock = lk::interprocess_lock::try_acquire (m_file, "payload");
      ASSERT_TRUE (lock.has_value ());
      EXPECT_FALSE (lk::interprocess_lock::try_acquire (m_file).has_value ());
      EXPECT_TRUE (lk::interprocess_lock::read_payload (m_file).has_value ());
      if (i % 2 == 0)
        lock.value ().release ();
    }

  // A leak is one per cycle at least: 100.
  EXPECT_LT (open_handle_count () - before, 16);
}

#if defined(_WIN32)
// The bug: the bytes that are not UTF-8 were skipped on the way to Windows,
// so the lock of "a\xFF" "b" was the lock of the file "ab".
TEST_F (
    LumexInterprocessLockTest,
    GivenInvalidUtf8Name_WhenTryAcquire_ThenRefusedAndTheOtherFileIsNotLocked)
{
  path const plain = m_base / "ab";
  path const bad = m_base / bad_ab ();
  auto held = lk::interprocess_lock::try_acquire (plain, "plain");
  ASSERT_TRUE (held.has_value ());

  auto const lock = lk::interprocess_lock::try_acquire (bad, "bad");

  ASSERT_FALSE (lock.has_value ());
  // Not busy: it is not the file "ab".
  EXPECT_EQ (lock.error ().kind, lk::lock_error::io_error);
  EXPECT_EQ (lock.error ().system_code, ERROR_INVALID_NAME);
  EXPECT_EQ (lk::interprocess_lock::read_payload (plain).value (), "plain");
}

TEST_F (
    LumexInterprocessLockTest,
    GivenInvalidUtf8Directory_WhenTryAcquire_ThenRefusedAndNoDirectoryIsMade)
{
  path const bad = m_base / (std::string ("d\xFF") + "e") / "device_a.lock";

  auto const lock = lk::interprocess_lock::try_acquire (bad, "bad");

  ASSERT_FALSE (lock.has_value ());
  EXPECT_EQ (lock.error ().kind, lk::lock_error::io_error);
  EXPECT_EQ (lock.error ().system_code, ERROR_INVALID_NAME);
  EXPECT_FALSE (lumex_filesystem::exists (m_base / "de"));
}

TEST_F (
    LumexInterprocessLockTest,
    GivenInvalidUtf8Name_WhenReadPayload_ThenRefusedAndTheOtherFileIsNotRead)
{
  auto held = lk::interprocess_lock::try_acquire (m_base / "ab", "plain");
  ASSERT_TRUE (held.has_value ());

  auto const payload
      = lk::interprocess_lock::read_payload (m_base / bad_ab ());

  ASSERT_FALSE (payload.has_value ());
  EXPECT_EQ (payload.error ().kind, lk::lock_error::io_error);
  EXPECT_EQ (payload.error ().system_code, ERROR_INVALID_NAME);
}
#else
// POSIX takes the bytes of a name as they are: there is no check.
TEST_F (LumexInterprocessLockTest,
        GivenNameThatIsNotUtf8_OnPosix_WhenTryAcquire_ThenItIsAnotherFile)
{
  path const plain = m_base / "ab";
  path const bad = m_base / bad_ab ();
  auto held = lk::interprocess_lock::try_acquire (plain, "plain");
  ASSERT_TRUE (held.has_value ());

  auto const lock = lk::interprocess_lock::try_acquire (bad, "bad");

  ASSERT_TRUE (lock.has_value ());
  EXPECT_EQ (lk::interprocess_lock::read_payload (bad).value (), "bad");
  EXPECT_EQ (lk::interprocess_lock::read_payload (plain).value (), "plain");
}
#endif

// The reason flock is used and not fcntl: a fcntl record lock belongs to the
// process and goes away when the process closes any descriptor of the file
// (a log reader, a backup tool of the same process).
TEST_F (LumexInterprocessLockTest,
        GivenHeldLock_WhenAnotherDescriptorOfTheFileIsClosed_ThenItIsStillHeld)
{
  auto lock = lk::interprocess_lock::try_acquire (m_file, "owner");
  ASSERT_TRUE (lock.has_value ());

#if defined(_WIN32)
  HANDLE const other
      = CreateFileW (m_file.wstring ().c_str (), GENERIC_READ,
                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                     nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  ASSERT_NE (other, INVALID_HANDLE_VALUE);
  CloseHandle (other);
#else
  int const other = open (m_file.c_str (), O_RDONLY | O_CLOEXEC);
  ASSERT_GE (other, 0);
  close (other);
#endif

  auto const second = lk::interprocess_lock::try_acquire (m_file);
  ASSERT_FALSE (second.has_value ());
  EXPECT_EQ (second.error ().kind, lk::lock_error::busy);
}

// A second process holds the lock: this one is refused across processes and
// reads the payload of the other; when the other is killed (no destructor, no
// unlock), the system releases its lock.
TEST_F (
    LumexInterprocessLockTest,
    GivenAnotherProcessHoldsTheLock_WhenItIsKilled_ThenBusyBeforeAndFreeAfter)
{
  lock_holding_process holder;
  ASSERT_TRUE (holder.start (m_file));
  // The payload of the helper also proves that it locked this very file.
  ASSERT_TRUE (wait_for_payload (m_file, child_payload))
      << "the helper process did not take the lock (running: "
      << holder.running () << ")";

  auto const refused = lk::interprocess_lock::try_acquire (m_file, "parent");
  ASSERT_FALSE (refused.has_value ());
  EXPECT_EQ (refused.error ().kind, lk::lock_error::busy);
  EXPECT_EQ (refused.error ().system_code, busy_code);
  EXPECT_EQ (lk::interprocess_lock::read_payload (m_file).value (),
             child_payload);

  holder.kill ();

  EXPECT_TRUE (acquire_within_seconds (m_file));
}

// The helper of the test above on Windows: CreateProcessW runs this binary
// with a filter on this test and the lock file in hold_file_variable. Never
// runs in an ordinary run (DISABLED_).
TEST_F (LumexInterprocessLockTest, DISABLED_HelperOfAnotherProcess)
{
  char const *const file = std::getenv (hold_file_variable);
  if (file == nullptr)
    GTEST_SKIP () << hold_file_variable << " is not set";
  hold_until_killed (path (file));
}

#if GTEST_HAS_DEATH_TEST
namespace
{
// The lock file of a death test. The child of a threadsafe death test runs the
// test again from the start, SetUp included, so m_base (it holds the process
// id) is another directory there: a lock taken in it would prove nothing.
// This name is the same in the parent and in the child, and differs between
// the tests and between the suites of the standards, which CTest may run at
// the same time.
path
death_test_directory ()
{
  return path (
      std::string ("lumex_interprocess_exit_")
      + ::testing::UnitTest::GetInstance ()->current_test_info ()->name ()
      + "_" + std::to_string (static_cast<long> (__cplusplus)));
}

// The child: takes the lock with `payload` and ends without releasing it
// (no destructor runs after _Exit). 42 only when it held the lock.
void
take_and_exit (path const &file, char const *payload)
{
  auto lock = lk::interprocess_lock::try_acquire (file, payload);
  std::_Exit (lock.has_value () ? 42 : 1);
}
} // namespace

// The guard against a death test that passes without testing anything: the
// child must lock the file the parent then looks at. The parent reads the
// payload of the child before it takes the lock itself; with a path that
// differs between the two processes there would be no such file.
TEST_F (LumexInterprocessLockTest,
        GivenDeathTestChild_WhenItTookTheLock_ThenTheParentReadsItsPayload)
{
  // threadsafe: the child runs this binary again from the start, so it does
  // not inherit a half-copied state of a running GoogleTest.
  GTEST_FLAG_SET (death_test_style, "threadsafe");
  path const directory = death_test_directory ();
  path const file = directory / "device_a.lock";
  lumex_filesystem::remove_all (directory);

  EXPECT_EXIT (take_and_exit (file, "written by the child"),
               ::testing::ExitedWithCode (42), "");

  auto const payload = lk::interprocess_lock::read_payload (file);
  ASSERT_TRUE (payload.has_value ())
      << "the parent does not see the file of the child";
  EXPECT_EQ (payload.value (), "written by the child");
  lumex_filesystem::remove_all (directory);
}

// A process that ends without releasing: the system releases the lock. On
// Windows the release of a process that did not unlock may come a little
// later, so the parent tries for a while.
TEST_F (LumexInterprocessLockTest,
        GivenProcessEndsWithoutReleasing_WhenItIsGone_ThenTheLockIsFree)
{
  GTEST_FLAG_SET (death_test_style, "threadsafe");
  path const directory = death_test_directory ();
  path const file = directory / "device_a.lock";
  lumex_filesystem::remove_all (directory);

  EXPECT_EXIT (take_and_exit (file, "child"), ::testing::ExitedWithCode (42),
               "");

  EXPECT_TRUE (acquire_within_seconds (file));
  lumex_filesystem::remove_all (directory);
}
#endif
