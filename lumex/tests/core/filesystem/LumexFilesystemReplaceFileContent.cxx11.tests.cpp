// lumex/tests/core/filesystem/LumexFilesystemReplaceFileContent.cxx11.tests.cpp
#include <cerrno>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#if defined(__unix__) || defined(__linux__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/utility/process/LumexProcess.hpp"

using lumex::core::filesystem::fs::KTEMPORARY_FILE_SUFFIX;
using lumex::core::filesystem::fs::write_mode;

namespace
{
// Bytes of a file that a failed write must leave unchanged. The carriage
// return makes a rewrite in text mode visible on every platform.
char const *const OLD_BYTES = "old\r\ncontent\n";

// Content a successful write puts into the file.
char const *const NEW_BYTES = "new\ncontent\r\n";

using lumex::core::filesystem::fs::lumex_filesystem;

void
write_bytes (lumex::path const &path, std::string const &bytes)
{
  std::ofstream file (path.string (), std::ios::binary);
  ASSERT_TRUE (file.is_open ()) << path.string ();
  file << bytes;
  file.close ();
  ASSERT_TRUE (file.good ()) << path.string ();
}

std::string
read_bytes (lumex::path const &path)
{
  std::ifstream file (path.string (), std::ios::binary);
  return std::string ((std::istreambuf_iterator<char> (file)),
                      std::istreambuf_iterator<char> ());
}

lumex::path
temporary_of (lumex::path const &path)
{
  return lumex::path (path.string () + KTEMPORARY_FILE_SUFFIX);
}

// A directory per test, removed (after its permissions are restored) when
// the test ends.
class ScratchDirectory
{
public:
  ScratchDirectory ()
  {
    ::testing::TestInfo const *info
        = ::testing::UnitTest::GetInstance ()->current_test_info ();
    std::string name = "test_replace_file_content_";
    name += info->test_suite_name ();
    name += "_";
    name += info->name ();
    // The suite of every standard runs the same case in the same working
    // directory, possibly at the same time: the process id keeps them apart.
    name += "_";
    name += std::to_string (lumex::core::utility::process::get_current_pid ());
    for (auto &chr : name)
      if (chr == '/')
        chr = '_';
    m_dir = lumex::path (name);
    remove ();
    if (!lumex_filesystem::create_directories (m_dir).success ())
      std::cerr << "Warning: Failed to create test directory: " << m_dir
                << std::endl;
  }

  ~ScratchDirectory () { remove (); }

  ScratchDirectory (ScratchDirectory const &) = delete;
  ScratchDirectory &operator= (ScratchDirectory const &) = delete;

  lumex::path const &
  path () const
  {
    return m_dir;
  }

private:
  void
  remove ()
  {
    if (!lumex_filesystem::exists (m_dir))
      return;
#if LUMEX_OS_UNIX
    (void)chmod (m_dir.c_str (), 0777);
#endif
    if (!lumex_filesystem::remove_all (m_dir).success ())
      std::cerr << "Warning: Failed to remove test directory: " << m_dir
                << std::endl;
  }

  lumex::path m_dir;
};

#if LUMEX_OS_UNIX
// True when permission bits do not restrict this process (root).
bool
permissions_are_ignored ()
{
  return ::geteuid () == 0;
}
#endif
} // namespace

// --- replace_file_content ---------------------------------------------------

TEST (LumexFilesystemReplaceFileContentTest,
      GivenMissingFile_WhenReplaceFileContent_ThenCreatesItWithTheBytes)
{
  ScratchDirectory dir;
  lumex::path const file = dir.path () / "target.cfg";

  EXPECT_TRUE (lumex_filesystem::replace_file_content (file, NEW_BYTES,
                                                       write_mode::binary)
                   .success ());
  EXPECT_EQ (read_bytes (file), NEW_BYTES);
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (file)));
}

TEST (
    LumexFilesystemReplaceFileContentTest,
    GivenExistingFile_WhenReplaceFileContent_ThenReplacesItAndLeavesNoTemporary)
{
  ScratchDirectory dir;
  lumex::path const file = dir.path () / "target.cfg";
  write_bytes (file, OLD_BYTES);

  EXPECT_TRUE (lumex_filesystem::replace_file_content (file, NEW_BYTES,
                                                       write_mode::binary)
                   .success ());
  EXPECT_EQ (read_bytes (file), NEW_BYTES);
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (file)));
}

TEST (LumexFilesystemReplaceFileContentTest,
      GivenEmptyContent_WhenReplaceFileContent_ThenFileBecomesEmpty)
{
  ScratchDirectory dir;
  lumex::path const file = dir.path () / "target.cfg";
  write_bytes (file, OLD_BYTES);

  EXPECT_TRUE (
      lumex_filesystem::replace_file_content (file, "", write_mode::binary)
          .success ());
  EXPECT_EQ (read_bytes (file), "");
}

TEST (LumexFilesystemReplaceFileContentTest,
      GivenMissingParent_WhenReplaceFileContent_ThenCreatesDirectories)
{
  ScratchDirectory dir;
  lumex::path const file = dir.path () / "nested" / "deeper" / "target.cfg";

  EXPECT_TRUE (lumex_filesystem::replace_file_content (file, NEW_BYTES,
                                                       write_mode::binary)
                   .success ());
  EXPECT_EQ (read_bytes (file), NEW_BYTES);
}

TEST (LumexFilesystemReplaceFileContentTest,
      GivenStaleTemporaryFile_WhenReplaceFileContent_ThenOverwritesIt)
{
  ScratchDirectory dir;
  lumex::path const file = dir.path () / "target.cfg";
  write_bytes (temporary_of (file), "left over by an interrupted write");

  EXPECT_TRUE (lumex_filesystem::replace_file_content (file, NEW_BYTES,
                                                       write_mode::binary)
                   .success ());
  EXPECT_EQ (read_bytes (file), NEW_BYTES);
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (file)));
}

TEST (LumexFilesystemReplaceFileContentTest,
      GivenEmptyPath_WhenReplaceFileContent_ThenReturnsEinval)
{
  auto const result = lumex_filesystem::replace_file_content (
      lumex::path (""), NEW_BYTES, write_mode::binary);
  EXPECT_FALSE (result.success ());
  EXPECT_EQ (result.error_code (), EINVAL);
}

TEST (
    LumexFilesystemReplaceFileContentTest,
    GivenDirectoryAtTemporaryPath_WhenReplaceFileContent_ThenReturnsFalseAndKeepsTheFile)
{
  ScratchDirectory dir;
  lumex::path const file = dir.path () / "target.cfg";
  write_bytes (file, OLD_BYTES);
  ASSERT_TRUE (
      lumex_filesystem::create_directories (temporary_of (file)).success ());

  auto const result = lumex_filesystem::replace_file_content (
      file, NEW_BYTES, write_mode::binary);
  EXPECT_FALSE (result.success ());
  EXPECT_NE (result.error_code (), 0);
  EXPECT_EQ (read_bytes (file), OLD_BYTES);
  EXPECT_TRUE (lumex_filesystem::is_directory (temporary_of (file)));
}

TEST (
    LumexFilesystemReplaceFileContentTest,
    GivenDirectoryAtTargetPath_WhenReplaceFileContent_ThenReturnsFalseAndRemovesTheTemporary)
{
  ScratchDirectory dir;
  lumex::path const target = dir.path () / "target.cfg";
  ASSERT_TRUE (lumex_filesystem::create_directories (target).success ());

  EXPECT_FALSE (lumex_filesystem::replace_file_content (target, NEW_BYTES,
                                                        write_mode::binary)
                    .success ());
  EXPECT_TRUE (lumex_filesystem::is_directory (target));
  EXPECT_FALSE (lumex_filesystem::exists (temporary_of (target)));
}

TEST (
    LumexFilesystemReplaceFileContentTest,
    GivenReadOnlyDirectory_WhenReplaceFileContent_ThenReturnsFalseAndKeepsTheFile)
{
#if LUMEX_OS_UNIX
  if (permissions_are_ignored ())
    GTEST_SKIP () << "running as root: permission bits do not deny writing";
  ScratchDirectory dir;
  lumex::path const file = dir.path () / "target.cfg";
  write_bytes (file, OLD_BYTES);
  ASSERT_EQ (chmod (dir.path ().c_str (), 0555), 0);

  bool const result = lumex_filesystem::replace_file_content (
                          file, NEW_BYTES, write_mode::binary)
                          .success ();
  (void)chmod (dir.path ().c_str (), 0777);

  EXPECT_FALSE (result);
  EXPECT_EQ (read_bytes (file), OLD_BYTES);
#else
  GTEST_SKIP () << "directory permission bits are POSIX-only";
#endif
}
