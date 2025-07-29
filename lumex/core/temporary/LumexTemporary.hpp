#ifndef LUMEX_TEMPORARY_HPP
#define LUMEX_TEMPORARY_HPP

#include "lumex/LumexExport.hpp"
#include "lumex/core/environment/LumexEnvironment"
#include "lumex/core/filesystem/LumexFilesystem"
#include "lumex/core/utility/LumexUtility"

#include <string>

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    namespace Temporary
    {
      /**
       * @brief RAII wrapper for temporary directory management
       * @details Automatically removes temporary directory when destroyed
       */
      class LUMEX_API TemporaryDirectory
      {
      public:
        TemporaryDirectory() : m_valid(false) {}
        explicit TemporaryDirectory(Lumex::Path const &path);
        ~TemporaryDirectory();

        // Non-copyable, movable
        TemporaryDirectory(TemporaryDirectory const &)            = delete;
        TemporaryDirectory &operator=(TemporaryDirectory const &) = delete;

        TemporaryDirectory(TemporaryDirectory &&other) noexcept;
        TemporaryDirectory &operator=(TemporaryDirectory &&other) noexcept;

        Lumex::Path const &
        path() const
        {
          return m_path;
        }
        bool
        is_valid() const
        {
          return m_valid;
        }
        void release(); // Don't auto-remove on destruction

      private:
        Lumex::Path m_path; ///< Path to the temporary directory.
        bool m_valid;       ///< Flag indicating if the temporary directory is valid.
      };

      /**
       * @brief RAII wrapper for temporary file management
       * @details Automatically removes temporary file when destroyed
       */
      class LUMEX_API TemporaryFile
      {
      public:
        TemporaryFile() : m_valid(false) {}
        explicit TemporaryFile(Lumex::Path const &path);
        ~TemporaryFile();

        // Non-copyable, movable
        TemporaryFile(TemporaryFile const &)            = delete;
        TemporaryFile &operator=(TemporaryFile const &) = delete;

        TemporaryFile(TemporaryFile &&other) noexcept;
        TemporaryFile &operator=(TemporaryFile &&other) noexcept;

        Lumex::Path const &
        path() const
        {
          return m_path;
        }
        bool
        is_valid() const
        {
          return m_valid;
        }
        void release(); // Don't auto-remove on destruction

      private:
        Lumex::Path m_path;
        bool m_valid;
      };

      class LUMEX_API LumexTemporary
      {
      public:
        /**
         * @brief Returns the path to the temporary directory.
         * @return The path to the temporary directory.
         */
        static Lumex::Path get_temp_directory_path();

        /**
         * @brief Creates a temporary directory with optional name prefix
         * @param name Prefix for directory name (can be empty)
         * @return FilesystemResult containing TemporaryDirectory on success
         */
        static Lumex::FilesystemResult<TemporaryDirectory>
        create_temp_directory(std::string const &name = std::string());

        /**
         * @brief Removes a temporary directory
         * @param path Path to directory to remove
         * @return FilesystemResult indicating success/failure
         */
        static Lumex::FilesystemResult<void> remove_temp_directory(Lumex::Path const &path);

        /**
         * @brief Creates a temporary file with optional name prefix
         * @param name Prefix for file name (can be empty)
         * @return FilesystemResult containing TemporaryFile on success
         */
        static Lumex::FilesystemResult<TemporaryFile> create_temp_file(std::string const &name = std::string());

        /**
         * @brief Removes a temporary file
         * @param path Path to file to remove
         * @return FilesystemResult indicating success/failure
         */
        static Lumex::FilesystemResult<void> remove_temp_file(Lumex::Path const &path);

        /**
         * @brief Generate unique temporary name with prefix
         * @param prefix Optional prefix for the name
         * @return Unique string suitable for temporary files/directories
         */
        static std::string generate_temp_name(std::string const &prefix = std::string());

      private:
        // Helper methods
        static std::string _generate_random_suffix();
        static bool _ensure_temp_directory_exists(Lumex::Path const &temp_dir);
      };
    } // namespace Core
  } // namespace Temporary
} // namespace Lumex

using TemporaryFile      = Lumex::Core::Temporary::TemporaryFile;
using TemporaryDirectory = Lumex::Core::Temporary::TemporaryDirectory;

using LumexTemporary     = Lumex::Core::Temporary::LumexTemporary;

#endif // !LUMEX_TEMPORARY_HPP
