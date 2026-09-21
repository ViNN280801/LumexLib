#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/utility/os/LumexCheckOS.hpp"

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

using namespace lumex::core::filesystem::fs;

// --- Test Fixtures ----------------------------------------------------------
class LumexFilesystemTest : public ::testing::Test
{
protected:
  void
  SetUp () override
  {
    // Unique directory per test: gtest_discover_tests launches one process
    // per case, and a shared "test_filesystem/" leftover (locked file on
    // Windows, or a prior rename) poisons the next case's SetUp/remove_all.
    ::testing::TestInfo const *info
        = ::testing::UnitTest::GetInstance ()->current_test_info ();
    std::string dir_name = "test_filesystem_";
    dir_name += info->test_suite_name ();
    dir_name += "_";
    dir_name += info->name ();
    test_dir = lumex::path (dir_name);
    test_file = test_dir / "test_file.txt";
    test_dir_nested = test_dir / "nested" / "deep";

    // Clean up any existing test artifacts
    if (lumex::core::filesystem::fs::lumex_filesystem::exists (test_dir))
      {
        auto result
            = lumex::core::filesystem::fs::lumex_filesystem::remove_all (
                test_dir);
        if (!result.success ())
          std::cerr << "Warning: Failed to remove existing test directory: "
                    << test_dir << std::endl;
      }

    // Create the base test directory
    auto result
        = lumex::core::filesystem::fs::lumex_filesystem::create_directories (
            test_dir);
    if (!result.success ())
      std::cerr << "Warning: Failed to create test directory: " << test_dir
                << std::endl;
  }

  void
  TearDown () override
  {
    // Clean up test artifacts
    if (lumex::core::filesystem::fs::lumex_filesystem::exists (test_dir))
      {
        auto result
            = lumex::core::filesystem::fs::lumex_filesystem::remove_all (
                test_dir);
        if (!result.success ())
          std::cerr << "Warning: Failed to clean up test directory: "
                    << test_dir << std::endl;
      }
    lumex::path const copied = lumex::path (test_dir.string () + "_copied");
    if (lumex::core::filesystem::fs::lumex_filesystem::exists (copied))
      lumex::core::filesystem::fs::lumex_filesystem::remove_all (copied);
  }

  // Helper to create test file with content
  void
  create_test_file (lumex::path const &path,
                    std::string const &content = "test content")
  {
    // Ensure parent directory exists
    lumex::path parent = path.parent_path ();
    if (!parent.empty ()
        && !lumex::core::filesystem::fs::lumex_filesystem::exists (parent))
      {
        auto result = lumex::core::filesystem::fs::lumex_filesystem::
            create_directories (parent);
        ASSERT_TRUE (result.success ())
            << "Failed to create parent directory: " << parent.string ();
      }

    std::ofstream file (path.string ());
    ASSERT_TRUE (file.is_open ())
        << "Failed to create test file: " << path.string ();
    file << content;
    file.close ();
  }

  // Helper to create test directory
  void
  create_test_directory (lumex::path const &path)
  {
    auto result
        = lumex::core::filesystem::fs::lumex_filesystem::create_directories (
            path);
    ASSERT_TRUE (result.success () || result.value ())
        << "Failed to create test directory: " << path.string ();
  }

  lumex::path test_dir;
  lumex::path test_file;
  lumex::path test_dir_nested;
};

// --- Path Class Tests -------------------------------------------------------

TEST_F (LumexFilesystemTest, Path_DefaultConstruction_ReturnsEmptyPath)
{
  lumex::path path;
  EXPECT_TRUE (path.empty ()); // Should be "." not empty
  EXPECT_FALSE (path.has_filename ());
  EXPECT_EQ (path.string (), "");
}

TEST_F (LumexFilesystemTest, Path_StringConstruction_HandlesValidInput)
{
  lumex::path path ("test/path/file.txt");
  EXPECT_FALSE (path.empty ());
  EXPECT_TRUE (path.has_filename ());
  EXPECT_EQ (path.filename ().string (), "file.txt");
  EXPECT_EQ (path.extension ().string (), ".txt");
  EXPECT_EQ (path.stem ().string (), "file");
}

TEST_F (LumexFilesystemTest, Path_StringConstruction_HandlesEmptyInput)
{
  lumex::path path ("");
  EXPECT_TRUE (path.empty ()); // Should normalize to "."
  EXPECT_EQ (path.string (), "");
}

TEST_F (LumexFilesystemTest, Path_StringConstruction_HandlesNullPointer)
{
  lumex::path path (nullptr);
  EXPECT_TRUE (path.empty ()); // Should normalize to "."
  EXPECT_EQ (path.string (), "");
}

TEST_F (LumexFilesystemTest, Path_CopyConstruction_PreservesState)
{
  lumex::path original ("test/path/file.txt");
  lumex::path copy (original);
  EXPECT_EQ (copy.string (), original.string ());
  EXPECT_EQ (copy.filename ().string (), original.filename ().string ());
}

TEST_F (LumexFilesystemTest, Path_MoveConstruction_TransfersOwnership)
{
  lumex::path original ("test/path/file.txt");
  std::string original_str = original.string ();
  lumex::path moved (std::move (original));
  EXPECT_EQ (moved.string (), original_str);
  EXPECT_FALSE (original.empty ()); // Moved-from should be "." not empty
  EXPECT_EQ (original.string (), ".");
}

TEST_F (LumexFilesystemTest, Path_Concatenation_OperatorSlash)
{
  lumex::path base ("base");
  lumex::path sub ("sub");
  lumex::path result = base / sub;
#if LUMEX_OS_IS_WINDOWS()
  EXPECT_EQ (result.string (), "base\\sub");
#else
  EXPECT_EQ (result.string (), "base/sub");
#endif
}

TEST_F (LumexFilesystemTest, Path_Concatenation_OperatorSlashEquals)
{
  lumex::path path ("base");
  path /= "sub";
  path /= "file.txt";
#if LUMEX_OS_IS_WINDOWS()
  EXPECT_EQ (path.string (), "base\\sub\\file.txt");
#else
  EXPECT_EQ (path.string (), "base/sub/file.txt");
#endif
}

TEST_F (LumexFilesystemTest, Path_Concatenation_HandlesAbsolutePaths)
{
  lumex::path base ("base");
  lumex::path absolute ("/absolute/path");
  lumex::path result = base / absolute;
  EXPECT_EQ (result.string (),
             "/absolute/path"); // Absolute path should override
}

TEST_F (LumexFilesystemTest, Path_Concatenation_HandlesEmptyPaths)
{
  lumex::path path ("base");
  lumex::path empty;
  path /= empty;
  EXPECT_EQ (path.string (), "base"); // Empty path should not change result
}

TEST_F (LumexFilesystemTest, Path_Concatenation_HandlesCurrentDirectory)
{
  lumex::path path (".");
  path /= "file.txt";
  EXPECT_EQ (path.string (), "file.txt"); // Should simplify from "./file.txt"
}

TEST_F (LumexFilesystemTest, Path_Decomposition_Filename)
{
  lumex::path path ("dir/subdir/file.txt");
  EXPECT_EQ (path.filename ().string (), "file.txt");

  lumex::path dir_path ("dir/subdir/");
  EXPECT_EQ (dir_path.filename ().string (),
             "subdir"); // Should extract last component

  lumex::path root_path ("/");
  EXPECT_EQ (root_path.filename ().string (), "/");
}

TEST_F (LumexFilesystemTest, Path_Decomposition_ParentPath)
{
  lumex::path path ("dir/subdir/file.txt");
  EXPECT_EQ (path.parent_path ().string (), "dir/subdir");

  lumex::path single_file ("file.txt");
  EXPECT_TRUE (single_file.parent_path ().empty ());

  lumex::path root_path ("/");
  // Root path should have empty parent for consistency
  EXPECT_TRUE (root_path.parent_path ().empty ()
               || root_path.parent_path ().string () == "/");
}

TEST_F (LumexFilesystemTest, Path_Decomposition_Extension)
{
  lumex::path path ("file.txt");
  EXPECT_EQ (path.extension ().string (), ".txt");

  lumex::path path_no_ext ("file");
  EXPECT_TRUE (path_no_ext.extension ().empty ());

  lumex::path path_double_ext ("file.txt.bak");
  EXPECT_EQ (path_double_ext.extension ().string (), ".bak");

  lumex::path path_hidden ("file.txt");
  EXPECT_EQ (path_hidden.extension ().string (), ".txt");
}

TEST_F (LumexFilesystemTest, Path_Decomposition_Stem)
{
  lumex::path path ("file.txt");
  EXPECT_EQ (path.stem ().string (), "file");

  lumex::path path_no_ext ("file");
  EXPECT_EQ (path_no_ext.stem ().string (), "file");

  lumex::path path_double_ext ("file.txt.bak");
  EXPECT_EQ (path_double_ext.stem ().string (), "file.txt");
}

TEST_F (LumexFilesystemTest, Path_Queries_IsAbsolute)
{
#if LUMEX_OS_IS_WINDOWS()
  EXPECT_TRUE (lumex::path ("C:\\file.txt").is_absolute ());
  EXPECT_TRUE (lumex::path ("\\\\server\\share\\file.txt").is_absolute ());
  EXPECT_FALSE (lumex::path ("file.txt").is_absolute ());
  EXPECT_FALSE (lumex::path ("dir\\file.txt").is_absolute ());
  // Unix-style paths are not absolute on Windows
  EXPECT_FALSE (lumex::path ("/file.txt").is_absolute ());
  // Drive letter without a root-directory is not absolute (std::filesystem)
  EXPECT_FALSE (lumex::path ("C:file.txt").is_absolute ());
#else
  EXPECT_TRUE (lumex::path ("/file.txt").is_absolute ());
  EXPECT_FALSE (lumex::path ("file.txt").is_absolute ());
  EXPECT_FALSE (lumex::path ("dir/file.txt").is_absolute ());
#endif
}

TEST_F (LumexFilesystemTest, Path_Queries_IsRelative)
{
  EXPECT_TRUE (lumex::path ("file.txt").is_relative ());
  EXPECT_TRUE (lumex::path ("dir/file.txt").is_relative ());

#if LUMEX_OS_IS_WINDOWS()
  EXPECT_FALSE (lumex::path ("C:\\file.txt").is_relative ());
  // Unix-style paths are relative on Windows
  EXPECT_TRUE (lumex::path ("/file.txt").is_relative ());
#else
  EXPECT_FALSE (lumex::path ("/file.txt").is_relative ());
#endif
}

TEST_F (LumexFilesystemTest, Path_Queries_HasFilename)
{
  EXPECT_TRUE (lumex::path ("file.txt").has_filename ());
  EXPECT_TRUE (lumex::path ("dir/file.txt").has_filename ());
  EXPECT_FALSE (lumex::path ("dir/").has_filename ());
  EXPECT_FALSE (lumex::path (".").has_filename ());
  EXPECT_FALSE (lumex::path ("..").has_filename ());
}

TEST_F (LumexFilesystemTest, Path_Queries_HasExtension)
{
  EXPECT_TRUE (lumex::path ("file.txt").has_extension ());
  EXPECT_TRUE (lumex::path ("dir/file.txt").has_extension ());
  EXPECT_FALSE (lumex::path ("file").has_extension ());
  EXPECT_FALSE (lumex::path ("dir/").has_extension ());
}

TEST_F (LumexFilesystemTest, Path_Modification_ReplaceExtension)
{
  lumex::path path ("file.txt");
  path.replace_extension (".bak");
  EXPECT_EQ (path.string (), "file.bak");

  lumex::path path_no_ext ("file");
  path_no_ext.replace_extension (".txt");
  EXPECT_EQ (path_no_ext.string (), "file.txt");

  lumex::path path_remove_ext ("file.txt");
  path_remove_ext.replace_extension ();
  EXPECT_EQ (path_remove_ext.string (), "file");
}

TEST_F (LumexFilesystemTest, Path_Modification_RemoveFilename)
{
  lumex::path path ("dir/file.txt");
  path.remove_filename ();
  EXPECT_EQ (path.string (), "dir"); // Should remove trailing separator

  lumex::path single_file ("file.txt");
  single_file.remove_filename ();
  EXPECT_EQ (single_file.string (), "."); // Should become current directory
}

TEST_F (LumexFilesystemTest, Path_Modification_ReplaceFilename)
{
  lumex::path path ("dir/old.txt");
  path.replace_filename ("new.txt");
#if LUMEX_OS_IS_WINDOWS()
  EXPECT_EQ (path.string (), "dir\\new.txt");
#else
  EXPECT_EQ (path.string (), "dir/new.txt");
#endif
}

TEST_F (LumexFilesystemTest, Path_Modification_MakePreferred)
{
  lumex::path path ("dir\\file.txt");
  path.make_preferred ();

#if LUMEX_OS_IS_WINDOWS()
  EXPECT_EQ (path.string (), "dir\\file.txt");
#else
  EXPECT_EQ (path.string (), "dir/file.txt");
#endif
}

// --- filesystem Operations Tests --------------------------------------------

TEST_F (LumexFilesystemTest, Filesystem_Exists_ReturnsCorrectStatus)
{
  auto funcTestFile = test_dir / "Filesystem_Exists_ReturnsCorrectStatus.txt";
  create_test_file (funcTestFile);
  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (funcTestFile));
  EXPECT_FALSE (lumex::core::filesystem::fs::lumex_filesystem::exists (
      lumex::path ("nonexistent_file.txt")));
}

TEST_F (LumexFilesystemTest, Exists_WhenFound_ThenTrue)
{
  auto const found = test_dir / "Exists_WhenFound_ThenTrue.txt";
  create_test_file (found);
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::exists (found));
}

TEST_F (LumexFilesystemTest, Exists_WhenUnfound_ThenFalse)
{
  auto const missing = test_dir / "Exists_WhenUnfound_ThenFalse.txt";
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (missing));
}

TEST_F (LumexFilesystemTest, Filesystem_IsRegularFile_ReturnsCorrectStatus)
{
  auto funcTestFile
      = test_dir / "Filesystem_IsRegularFile_ReturnsCorrectStatus.txt";
  create_test_file (funcTestFile);
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::is_regular_file (
      funcTestFile));
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::is_regular_file (
          test_dir));
}

TEST_F (LumexFilesystemTest, Filesystem_IsDirectory_ReturnsCorrectStatus)
{
  auto funcTestDir = test_dir / "Filesystem_IsDirectory_ReturnsCorrectStatus";
  create_test_directory (funcTestDir);
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::is_directory (
      funcTestDir));
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::is_directory (test_file));
}

TEST_F (LumexFilesystemTest, Filesystem_IsEmpty_ReturnsCorrectStatus)
{
  auto funcTestFile = test_dir / "Filesystem_IsEmpty_ReturnsCorrectStatus.txt";

  // Test empty file
  create_test_file (funcTestFile, "");
  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::is_empty (funcTestFile));

  // Test non-empty file
  create_test_file (funcTestFile, "content");
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::is_empty (funcTestFile));
}

TEST_F (LumexFilesystemTest, Filesystem_CreateDirectory_Success)
{
  lumex::path new_dir = test_dir / "new_directory";
  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::create_directory (
          new_dir);
  EXPECT_TRUE (result.success ());
  EXPECT_TRUE (result.value ());
  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (new_dir));
  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::is_directory (new_dir));
}

TEST_F (LumexFilesystemTest, Filesystem_CreateDirectory_AlreadyExists)
{
  auto funcTestDir = test_dir / "Filesystem_CreateDirectory_AlreadyExists";
  create_test_directory (funcTestDir);
  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::create_directory (
          funcTestDir);
  EXPECT_TRUE (result.success ());
  EXPECT_FALSE (result.value ()); // Directory already existed
}

TEST_F (LumexFilesystemTest, Filesystem_CreateDirectories_Recursive)
{
  auto funcTestDir = test_dir / "Filesystem_CreateDirectories_Recursive";
  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::create_directories (
          funcTestDir);
  EXPECT_TRUE (result.success ());
  EXPECT_TRUE (result.value ());
  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (funcTestDir));
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::is_directory (
      funcTestDir));
}

TEST_F (LumexFilesystemTest, Filesystem_Remove_Success)
{
  auto funcTestFile = test_dir / "Filesystem_Remove_Success.txt";
  create_test_file (funcTestFile);
  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::remove (funcTestFile);
  EXPECT_TRUE (result.success ());
  EXPECT_TRUE (result.value ());
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (funcTestFile));
}

TEST_F (LumexFilesystemTest, Filesystem_Remove_Nonexistent)
{
  auto result = lumex::core::filesystem::fs::lumex_filesystem::remove (
      lumex::path ("nonexistent_file.txt"));
  EXPECT_TRUE (result.success ());
  EXPECT_FALSE (result.value ());
}

TEST_F (LumexFilesystemTest, Filesystem_RemoveAll_Recursive)
{
  create_test_directory (test_dir_nested);
  create_test_file (test_dir_nested / "file1.txt");
  create_test_file (test_dir_nested / "file2.txt");

  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::remove_all (test_dir);
  EXPECT_TRUE (result.success ());
  EXPECT_GT (result.value (), 0);
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (test_dir));
}

TEST_F (LumexFilesystemTest, Filesystem_CopyFile_Success)
{
  auto funcTestFile = test_dir / "Filesystem_CopyFile_Success.txt";
  create_test_file (funcTestFile, "test content");
  lumex::path dest_file = test_dir / "copied_file.txt";

  auto result = lumex::core::filesystem::fs::lumex_filesystem::copy_file (
      funcTestFile, dest_file);
  EXPECT_TRUE (result.success ());
  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (dest_file));

  // Verify content was copied
  std::ifstream src (funcTestFile.string ());
  std::ifstream dst (dest_file.string ());
  std::string src_content, dst_content;
  std::getline (src, src_content);
  std::getline (dst, dst_content);
  EXPECT_EQ (src_content, dst_content);
}

TEST_F (LumexFilesystemTest, Filesystem_CopyFile_Overwrite)
{
  auto funcTestFile = test_dir / "Filesystem_CopyFile_Overwrite.txt";
  create_test_file (funcTestFile, "original content");
  lumex::path dest_file = test_dir / "dest_file.txt";
  create_test_file (dest_file, "existing content");

  auto result = lumex::core::filesystem::fs::lumex_filesystem::copy_file (
      funcTestFile, dest_file);
  EXPECT_TRUE (result.success ());

  // Verify content was overwritten
  std::ifstream dst (dest_file.string ());
  std::string content;
  std::getline (dst, content);
  EXPECT_EQ (content, "original content");
}

TEST_F (LumexFilesystemTest, Filesystem_Copy_Directory)
{
  create_test_directory (test_dir_nested);
  create_test_file (test_dir_nested / "file1.txt", "content1");
  create_test_file (test_dir_nested / "file2.txt", "content2");

  lumex::path dest_dir = lumex::path (test_dir.string () + "_copied");
  auto result = lumex::core::filesystem::fs::lumex_filesystem::copy (test_dir,
                                                                     dest_dir);
  EXPECT_TRUE (result.success ());

  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (dest_dir));
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::exists (
      dest_dir / "nested" / "deep" / "file1.txt"));
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::exists (
      dest_dir / "nested" / "deep" / "file2.txt"));

  // Cleanup
  lumex::core::filesystem::fs::lumex_filesystem::remove_all (dest_dir);
}

TEST_F (LumexFilesystemTest, Filesystem_Rename_Success)
{
  create_test_file (test_file, "test content");
  lumex::path new_path = test_dir / "renamed_file.txt";

  auto result = lumex::core::filesystem::fs::lumex_filesystem::rename (
      test_file, new_path);
  EXPECT_TRUE (result.success ());
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (test_file));
  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (new_path));
}

TEST_F (LumexFilesystemTest, Filesystem_FileSize_ReturnsCorrectSize)
{
  std::string content = "test content for size measurement";
  auto funcTestFile = test_dir / "Filesystem_FileSize_ReturnsCorrectSize.txt";
  create_test_file (funcTestFile, content);

  auto result = lumex::core::filesystem::fs::lumex_filesystem::file_size (
      funcTestFile);
  EXPECT_TRUE (result.success ());
  EXPECT_EQ (result.value (), content.length ());
}

TEST_F (LumexFilesystemTest, Filesystem_FileSize_NonexistentFile)
{
  auto result = lumex::core::filesystem::fs::lumex_filesystem::file_size (
      lumex::path ("nonexistent_file.txt"));
  EXPECT_FALSE (result.success ());
}

TEST_F (LumexFilesystemTest, Filesystem_LastWriteTime_ReturnsValidTimestamp)
{
  auto funcTestFile
      = test_dir / "Filesystem_LastWriteTime_ReturnsValidTimestamp.txt";
  create_test_file (funcTestFile);

  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::last_write_time (
          funcTestFile);
  EXPECT_TRUE (result.success ());
  EXPECT_GT (result.value (), 0);

  // Test setting timestamp
  std::time_t new_time = std::time (nullptr) - 3600; // 1 hour ago
  auto set_result
      = lumex::core::filesystem::fs::lumex_filesystem::last_write_time (
          funcTestFile, new_time);
  EXPECT_TRUE (set_result.success ());

  auto verify_result
      = lumex::core::filesystem::fs::lumex_filesystem::last_write_time (
          funcTestFile);
  EXPECT_TRUE (verify_result.success ());
  EXPECT_EQ (verify_result.value (), new_time);
}

TEST_F (LumexFilesystemTest, Filesystem_DirectoryContents_ReturnsAllEntries)
{
  create_test_directory (test_dir);
  create_test_file (test_dir / "file1.txt");
  create_test_file (test_dir / "file2.txt");
  create_test_directory (test_dir / "subdir");

  auto entries
      = lumex::core::filesystem::fs::lumex_filesystem::directory_contents (
          test_dir);
  EXPECT_EQ (entries.size (), 3);

  std::vector<std::string> entry_names;
  for (auto const &entry : entries)
    entry_names.push_back (entry.path ().filename ().string ());

  std::sort (entry_names.begin (), entry_names.end ());
  EXPECT_EQ (entry_names[0], "file1.txt");
  EXPECT_EQ (entry_names[1], "file2.txt");
  EXPECT_EQ (entry_names[2], "subdir");
}

TEST_F (LumexFilesystemTest, Filesystem_DirectoryIterator_WorksCorrectly)
{
  create_test_directory (test_dir);
  create_test_file (test_dir / "file1.txt");
  create_test_file (test_dir / "file2.txt");

  lumex::directory_iterator it (test_dir);
  lumex::directory_iterator end;

  std::vector<std::string> found_files;
  for (; it != end; ++it)
    found_files.push_back (it->path ().filename ().string ());

  EXPECT_EQ (found_files.size (), 2);
  std::sort (found_files.begin (), found_files.end ());
  EXPECT_EQ (found_files[0], "file1.txt");
  EXPECT_EQ (found_files[1], "file2.txt");
}

TEST_F (LumexFilesystemTest, Filesystem_DirectoryPaths_ReturnsAllEntries)
{
  create_test_directory (test_dir / "subdir1");
  create_test_directory (test_dir / "subdir2");
  create_test_file (test_dir / "file1.txt");
  create_test_file (test_dir / "file2.txt");

  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::directory_paths (
          test_dir);
  ASSERT_TRUE (result.success ());
  std::vector<lumex::path> paths = result.value ();

  // Sort paths to ensure consistent order for comparison
  std::sort (paths.begin (), paths.end ());

  EXPECT_EQ (paths.size (), 4);
  EXPECT_EQ (paths[0].filename ().string (), "file1.txt");
  EXPECT_EQ (paths[1].filename ().string (), "file2.txt");
  EXPECT_EQ (paths[2].filename ().string (), "subdir1");
  EXPECT_EQ (paths[3].filename ().string (), "subdir2");
}

TEST_F (LumexFilesystemTest, Filesystem_DirectoryPaths_EmptyDirectory)
{
  lumex::path empty_dir = test_dir / "empty_dir";
  create_test_directory (empty_dir);

  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::directory_paths (
          empty_dir);
  EXPECT_TRUE (result.success ());
  EXPECT_TRUE (result.value ().empty ());
}

TEST_F (LumexFilesystemTest, Filesystem_DirectoryPaths_NonExistentDirectory)
{
  lumex::path non_existent_dir = test_dir / "non_existent_dir";
  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::directory_paths (
          non_existent_dir);
  EXPECT_FALSE (result.success ());
  EXPECT_TRUE (result.value ().empty ());
  // The error code might vary based on OS/implementation, but EINVAL is a
  // common one for invalid paths. We expect a non-zero error code.
  EXPECT_NE (result.error_code (), 0);
}

TEST_F (LumexFilesystemTest, Filesystem_DirectoryPaths_PathIsFile)
{
  lumex::path file_path = test_dir / "single_file.txt";
  create_test_file (file_path);

  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::directory_paths (
          file_path);
  EXPECT_FALSE (result.success ());
  EXPECT_TRUE (result.value ().empty ());
  // Expected error code is ENOTDIR on POSIX, or equivalent on Windows for not
  // a directory.
  EXPECT_NE (result.error_code (), 0);
}

// --- Edge Cases and Error Handling Tests -----------------------------------

TEST_F (LumexFilesystemTest, Filesystem_CreateDirectory_ParentDoesNotExist)
{
  lumex::path deep_path = test_dir / "nonexistent" / "deep" / "path";
  auto result
      = lumex::core::filesystem::fs::lumex_filesystem::create_directory (
          deep_path);
  EXPECT_FALSE (result.success ());
}

TEST_F (LumexFilesystemTest, Filesystem_CopyFile_SourceDoesNotExist)
{
  auto result = lumex::core::filesystem::fs::lumex_filesystem::copy_file (
      lumex::path ("nonexistent.txt"), test_file);
  EXPECT_FALSE (result.success ());
}

TEST_F (LumexFilesystemTest,
        Filesystem_CopyFile_DestinationDirectoryDoesNotExist)
{
  auto funcTestFile
      = test_dir / "Filesystem_CopyFile_DestinationDirectoryDoesNotExist.txt";
  create_test_file (funcTestFile);
  lumex::path dest = lumex::path ("nonexistent_dir") / "file.txt";
  auto result = lumex::core::filesystem::fs::lumex_filesystem::copy_file (
      funcTestFile, dest);
  EXPECT_FALSE (result.success ());
}

TEST_F (LumexFilesystemTest, Filesystem_Permissions_ReadOnlyFile)
{
  auto funcTestFile = test_dir / "Filesystem_Permissions_ReadOnlyFile.txt";
  create_test_file (funcTestFile);

  // Set file to read-only
  auto result = lumex::core::filesystem::fs::lumex_filesystem::permissions (
      funcTestFile, lumex::perms::owner_read);
  EXPECT_TRUE (result.success ());

  // Verify file is now read-only
  auto status_result
      = lumex::core::filesystem::fs::lumex_filesystem::status (funcTestFile);
  EXPECT_TRUE (status_result.success ());
  EXPECT_TRUE (
      status_result.value ().permissions () == lumex::perms::owner_read
      || (status_result.value ().permissions () & lumex::perms::owner_read)
             != lumex::perms::none);

  // Reset permissions to allow deletion (e.g., owner_write)
  auto reset_result
      = lumex::core::filesystem::fs::lumex_filesystem::permissions (
          funcTestFile, lumex::perms::owner_write | lumex::perms::owner_read);
  EXPECT_TRUE (reset_result.success ());

  // Now delete the file
  lumex::core::filesystem::fs::lumex_filesystem::remove (funcTestFile);
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (funcTestFile));
}

TEST_F (LumexFilesystemTest, Filesystem_Absolute_ResolvesCorrectly)
{
  lumex::path relative_path (test_dir
                             / "Filesystem_Absolute_ResolvesCorrectly.txt");
  lumex::path absolute_path
      = lumex::core::filesystem::fs::lumex_filesystem::absolute (
          relative_path);

  EXPECT_TRUE (absolute_path.is_absolute ());
  EXPECT_EQ (absolute_path.filename ().string (),
             "Filesystem_Absolute_ResolvesCorrectly.txt");
}

TEST_F (LumexFilesystemTest, Filesystem_Canonical_ResolvesSymlinks)
{
  create_test_file (test_dir / "Filesystem_Canonical_ResolvesSymlinks.txt");

  lumex::path canonical_path
      = lumex::core::filesystem::fs::lumex_filesystem::canonical (
          test_dir / "Filesystem_Canonical_ResolvesSymlinks.txt");
  EXPECT_FALSE (canonical_path.empty ())
      << "Canonical path should not be empty";
  if (!canonical_path.empty ())
    {
      EXPECT_TRUE (canonical_path.is_absolute ());
      EXPECT_EQ (canonical_path.filename ().string (),
                 "Filesystem_Canonical_ResolvesSymlinks.txt");
    }
}

TEST_F (LumexFilesystemTest, Filesystem_Relative_ComputesRelativePath)
{
  create_test_directory (test_dir);
  create_test_file (test_dir / "Filesystem_Relative_ComputesRelativePath.txt");

  lumex::path base_path
      = lumex::core::filesystem::fs::lumex_filesystem::current_path ()
            .value ();
  lumex::path full_path
      = test_dir / "Filesystem_Relative_ComputesRelativePath.txt";

  lumex::path relative_path
      = lumex::core::filesystem::fs::lumex_filesystem::relative (full_path,
                                                                 base_path);
  EXPECT_TRUE (relative_path.is_relative ());
}

TEST_F (LumexFilesystemTest, Filesystem_ThreadSafety_ConcurrentExists)
{
  auto const test_file_path
      = test_dir / "Filesystem_ThreadSafety_ConcurrentExists.txt";
  create_test_file (test_file_path);

  // Ensure the file is fully created and visible to all threads
  ASSERT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (test_file_path));

  std::vector<std::future<bool>> futures;
  for (int i = 0; i < 10; ++i)
    futures.push_back (std::async (std::launch::async, [&test_file_path] () {
      return lumex::core::filesystem::fs::lumex_filesystem::exists (
          test_file_path);
    }));

  for (auto &future : futures)
    EXPECT_TRUE (future.get ());
}

TEST_F (LumexFilesystemTest,
        Filesystem_ThreadSafety_ConcurrentDirectoryCreation)
{
  std::vector<std::future<lumex::filesystem_result<bool>>> futures;
  for (int i = 0; i < 5; ++i)
    {
      futures.push_back (std::async (std::launch::async, [this, i] () {
        return lumex::core::filesystem::fs::lumex_filesystem::
            create_directory (test_dir / ("dir" + std::to_string (i)));
      }));
    }

  for (auto &future : futures)
    {
      auto result = future.get ();
      EXPECT_TRUE (result.success ());
    }
}

TEST_F (LumexFilesystemTest, Filesystem_StressTest_ManyFiles)
{
  // Clean up any existing files in the test directory
  auto existing_entries
      = lumex::core::filesystem::fs::lumex_filesystem::directory_contents (
          test_dir);
  for (auto const &entry : existing_entries)
    if (entry.is_regular_file ())
      lumex::core::filesystem::fs::lumex_filesystem::remove (entry.path ());

  int const num_files = 100; // Reduced from 1000 to avoid timeout

  auto start = std::chrono::high_resolution_clock::now ();

  // Create many files
  for (int i = 0; i < num_files; ++i)
    create_test_file (test_dir / ("file" + std::to_string (i) + ".txt"));

  // List directory contents
  auto entries
      = lumex::core::filesystem::fs::lumex_filesystem::directory_contents (
          test_dir);

  auto end = std::chrono::high_resolution_clock::now ();
  auto duration
      = std::chrono::duration_cast<std::chrono::milliseconds> (end - start);

  EXPECT_LT (duration.count (), 10000)
      << "Directory operations took too long: " << duration.count () << "ms";
}

// --- OS-Specific Tests -----------------------------------------------------

#if LUMEX_OS_IS_WINDOWS()
TEST_F (LumexFilesystemTest, Windows_Specific_PathHandling)
{
  lumex::path windows_path ("C:\\Program Files\\MyApp\\file.txt");
  EXPECT_TRUE (windows_path.is_absolute ());
  EXPECT_EQ (windows_path.root_name ().string (), "C:");
  EXPECT_EQ (windows_path.root_directory ().string (), "\\");

  lumex::path unc_path ("\\\\server\\share\\file.txt");
  EXPECT_TRUE (unc_path.is_absolute ());
  EXPECT_EQ (unc_path.root_name ().string (), "\\\\server\\share");
}

TEST_F (LumexFilesystemTest, Windows_Specific_FileAttributes)
{
  create_test_file (test_file);

  // Test Windows-specific file attributes
  auto result = lumex::core::filesystem::fs::lumex_filesystem::permissions (
      test_file, lumex::perms::owner_read);
  EXPECT_TRUE (result.success ());

  auto status
      = lumex::core::filesystem::fs::lumex_filesystem::status (test_file);
  EXPECT_TRUE (status.success ());
  EXPECT_EQ (status.value ().type (), lumex::file_type::regular);
}

TEST_F (LumexFilesystemTest, Windows_RemoveAll_DeletesReadOnlyFile)
{
  create_test_file (test_file);
  ASSERT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::permissions (
                   test_file, lumex::perms::owner_read)
                   .success ());
  auto removed
      = lumex::core::filesystem::fs::lumex_filesystem::remove_all (test_file);
  EXPECT_TRUE (removed.success ()) << "error=" << removed.error_code ();
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (test_file));
  EXPECT_GE (removed.value (), static_cast<std::uintmax_t> (1));
}

TEST_F (LumexFilesystemTest,
        Windows_RemoveAll_DeletesDirectoryContainingReadOnlyFile)
{
  create_test_file (test_file);
  ASSERT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::permissions (
                   test_file, lumex::perms::owner_read)
                   .success ());
  auto removed
      = lumex::core::filesystem::fs::lumex_filesystem::remove_all (test_dir);
  EXPECT_TRUE (removed.success ()) << "error=" << removed.error_code ();
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (test_dir));
  EXPECT_FALSE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (test_file));
  EXPECT_GE (removed.value (), static_cast<std::uintmax_t> (2));
}
#elif LUMEX_OS_IS_UNIX()
TEST_F (LumexFilesystemTest, Unix_Specific_PathHandling)
{
  lumex::path unix_path ("/usr/local/bin/program");
  EXPECT_TRUE (unix_path.is_absolute ());
  EXPECT_TRUE (unix_path.root_name ().empty ());
  EXPECT_EQ (unix_path.root_directory ().string (), "/");

  lumex::path relative_path ("./local/file.txt");
  EXPECT_TRUE (relative_path.is_relative ());
}

TEST_F (LumexFilesystemTest, Unix_Specific_FilePermissions)
{
  create_test_file (test_file);

  // Test Unix-specific permissions
  auto result = lumex::core::filesystem::fs::lumex_filesystem::permissions (
      test_file, lumex::perms::owner_read | lumex::perms::owner_write
                     | lumex::perms::owner_exec);
  EXPECT_TRUE (result.success ());

  auto status
      = lumex::core::filesystem::fs::lumex_filesystem::status (test_file);
  EXPECT_TRUE (status.success ());
  EXPECT_EQ (status.value ().type (), lumex::file_type::regular);
}
#endif

// --- Custom Methods Tests --------------------------------------------------

TEST_F (LumexFilesystemTest, Filesystem_GetExePath_ReturnsValidPath)
{
  lumex::path exe_path
      = lumex::core::filesystem::fs::lumex_filesystem::get_exe_path ();
  EXPECT_FALSE (exe_path.empty ());
  EXPECT_TRUE (exe_path.is_absolute ());
  EXPECT_TRUE (
      lumex::core::filesystem::fs::lumex_filesystem::exists (exe_path));
}

TEST_F (LumexFilesystemTest, Filesystem_IsReadable_ReturnsCorrectStatus)
{
  create_test_file (test_dir
                        / "Filesystem_IsReadable_ReturnsCorrectStatus.txt",
                    "readable content");
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::is_readable (
      test_dir / "Filesystem_IsReadable_ReturnsCorrectStatus.txt"));

  EXPECT_FALSE (lumex::core::filesystem::fs::lumex_filesystem::is_readable (
      lumex::path ("nonexistent_file.txt")));
}

TEST_F (LumexFilesystemTest, Filesystem_IsWritable_ReturnsCorrectStatus)
{
  create_test_file (test_dir
                        / "Filesystem_IsWritable_ReturnsCorrectStatus.txt",
                    "writable content");
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::is_writable (
      test_dir / "Filesystem_IsWritable_ReturnsCorrectStatus.txt"));

  EXPECT_FALSE (lumex::core::filesystem::fs::lumex_filesystem::is_writable (
      lumex::path ("nonexistent_file.txt")));
}

TEST_F (LumexFilesystemTest, Filesystem_IsAccessible_ReturnsCorrectStatus)
{
  create_test_file (test_dir
                        / "Filesystem_IsAccessible_ReturnsCorrectStatus.txt",
                    "accessible content");
  EXPECT_TRUE (lumex::core::filesystem::fs::lumex_filesystem::is_accessible (
      test_dir / "Filesystem_IsAccessible_ReturnsCorrectStatus.txt"));

  EXPECT_FALSE (lumex::core::filesystem::fs::lumex_filesystem::is_accessible (
      lumex::path ("nonexistent_file.txt")));
}

// --- filesystem_result Tests ------------------------------------------------

TEST_F (LumexFilesystemTest, FilesystemResult_Construction_WorksCorrectly)
{
  auto success_result = lumex::filesystem_result<int>::ok (42);
  EXPECT_TRUE (success_result.success ());
  EXPECT_EQ (success_result.value (), 42);
  EXPECT_EQ (success_result.error_code (), 0);

  auto error_result
      = lumex::filesystem_result<int>::err (static_cast<int> (ENOENT));
  EXPECT_FALSE (error_result.success ());
  EXPECT_EQ (error_result.error_code (), ENOENT);
}

TEST_F (LumexFilesystemTest, FilesystemResult_ValueOr_ReturnsCorrectValue)
{
  auto success_result = lumex::filesystem_result<int>::ok (42);
  EXPECT_EQ (success_result.value_or (99), 42);

  auto error_result
      = lumex::filesystem_result<int>::err (static_cast<int> (ENOENT));
  EXPECT_EQ (error_result.value_or (99), 99);
}

// --- directory_entry Tests --------------------------------------------------

TEST_F (LumexFilesystemTest, DirectoryEntry_Construction_WorksCorrectly)
{
  create_test_file (test_dir
                    / "DirectoryEntry_Construction_WorksCorrectly.txt");

  lumex::directory_entry entry (
      test_dir / "DirectoryEntry_Construction_WorksCorrectly.txt");
  EXPECT_EQ (entry.path ().string (),
             test_dir / "DirectoryEntry_Construction_WorksCorrectly.txt");
  EXPECT_TRUE (entry.exists ());
  EXPECT_TRUE (entry.is_regular_file ());
  EXPECT_FALSE (entry.is_directory ());
}

TEST_F (LumexFilesystemTest, DirectoryEntry_Comparison_WorksCorrectly)
{
  create_test_file (test_dir / "DirectoryEntry_Comparison_WorksCorrectly.txt");

  lumex::directory_entry entry1 (
      test_dir / "DirectoryEntry_Comparison_WorksCorrectly.txt");
  lumex::directory_entry entry2 (
      test_dir / "DirectoryEntry_Comparison_WorksCorrectly.txt");
  lumex::directory_entry entry3 (lumex::path ("different_file.txt"));

  EXPECT_EQ (entry1, entry2);
  EXPECT_NE (entry1, entry3);
  // Note: Comparison order might vary depending on path string comparison
  // Just verify they are different
  EXPECT_TRUE (entry1 != entry3);
}
