#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>

#include "Logger.hpp"

#if LOGGER_OS_IS_WINDOWS()
  #include <windows.h>
  #ifdef _MSC_VER
    #include <dbghelp.h>
  #endif
#else
  #include <dirent.h>
  #include <sys/stat.h>
  #include <sys/statvfs.h>
  #include <unistd.h>
  #if defined(__GNUC__) || defined(__clang__)
    #if __has_include(<execinfo.h>)
      #include <execinfo.h>
    #endif
    #if __has_include(<dlfcn.h>)
      #include <dlfcn.h>
    #endif
    #if __has_include(<cxxabi.h>)
      #include <cxxabi.h>
    #endif
  #endif
  #if defined(__APPLE__)
    #include <mach-o/dyld.h>
  #endif
  #if defined(__linux__)
    #include <linux/limits.h>
  #endif
#endif

namespace
{
  // ============= Utility Functions =============

  /**
   * @brief Преобразует интегральный тип или указатель в строку в формате 0xHH.
   */
  template <typename T>
  typename std::enable_if<std::is_integral<T>::value, std::string>::type
  formatHex(T value) LOGGER_NOEXCEPT_FUNCTION
  {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << value;
    return oss.str();
  }

  template <typename T>
  typename std::enable_if<std::is_pointer<T>::value, std::string>::type
  formatHex(T value) LOGGER_NOEXCEPT_FUNCTION
  {
    if(value == nullptr) return "0x00 (nullptr)";
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << reinterpret_cast<uintptr_t>(value);
    return oss.str();
  }

  /**
   * @brief Получает размер файла в байтах.
   */
  int64_t
  getFileSize(std::string const &filePath) LOGGER_NOEXCEPT_FUNCTION
  {
    try
    {
      if(filePath.empty()) return -1;

#if LOGGER_OS_IS_WINDOWS()
      WIN32_FILE_ATTRIBUTE_DATA fileData{};
      if(GetFileAttributesExA(filePath.c_str(), GetFileExInfoStandard, &fileData) == 0) return -1;

      if((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) return -1;

      LARGE_INTEGER fileSize{};
      fileSize.LowPart  = fileData.nFileSizeLow;
      fileSize.HighPart = fileData.nFileSizeHigh;
      return static_cast<int64_t>(fileSize.QuadPart);
#else
      struct stat fileStat{};
      if(stat(filePath.c_str(), &fileStat) != 0) return -1;

      if(!S_ISREG(fileStat.st_mode)) return -1;

      return static_cast<int64_t>(fileStat.st_size);
#endif
    }
    catch(...)
    {
      return -1;
    }
  }

  /**
   * @brief Получает свободное место на диске в байтах.
   */
  int64_t
  getFreeDiskSpace(std::string const &path) LOGGER_NOEXCEPT_FUNCTION
  {
    try
    {
      if(path.empty()) return -1;

#if LOGGER_OS_IS_WINDOWS()
      ULARGE_INTEGER freeBytesAvailable{};
      ULARGE_INTEGER totalNumberOfBytes{};
      ULARGE_INTEGER totalNumberOfFreeBytes{};

      std::wstring widePath(path.begin(), path.end());

      if(GetDiskFreeSpaceExW(widePath.c_str(), &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes))
      {
        return static_cast<int64_t>(freeBytesAvailable.QuadPart);
      }
      else
      {
        if(GetDiskFreeSpaceExA(path.c_str(), &freeBytesAvailable, &totalNumberOfBytes, &totalNumberOfFreeBytes))
          return static_cast<int64_t>(freeBytesAvailable.QuadPart);
        return -1;
      }
#else
      struct statvfs vfs{};
      if(statvfs(path.c_str(), &vfs) == 0)
      {
        int64_t freeSpace = static_cast<int64_t>(vfs.f_bavail) * static_cast<int64_t>(vfs.f_frsize);
        return freeSpace;
      }
      else { return -1; }
#endif
    }
    catch(...)
    {
      return -1;
    }
  }

  /**
   * @brief Получает общий размер всех файлов в директории в байтах.
   */
  int64_t
  getDirectorySize(std::string const &directoryPath) LOGGER_NOEXCEPT_FUNCTION
  {
    try
    {
      if(directoryPath.empty()) return -1;

      int64_t totalSize = 0;

#if LOGGER_OS_IS_WINDOWS()
      std::string searchPath = directoryPath + "\\*";

      WIN32_FIND_DATAA findData{};
      HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

      if(hFind == INVALID_HANDLE_VALUE) return -1;

      do {
        if(strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) continue;

        std::string fullPath = directoryPath + "\\" + findData.cFileName;

        if((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
          int64_t subDirSize = getDirectorySize(fullPath);
          if(subDirSize < 0) return -1;
          totalSize += subDirSize;
        }
        else
        {
          LARGE_INTEGER fileSize{};
          fileSize.LowPart  = findData.nFileSizeLow;
          fileSize.HighPart = findData.nFileSizeHigh;
          totalSize += static_cast<int64_t>(fileSize.QuadPart);
        }
      } while(FindNextFileA(hFind, &findData) != 0);

      FindClose(hFind);

#else
      DIR *dir = opendir(directoryPath.c_str());
      if(dir == nullptr) return -1;

      struct dirent *entry;
      while((entry = readdir(dir)) != nullptr)
      {
        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

        std::string fullPath = directoryPath + "/" + entry->d_name;

        struct stat entryStat{};
        if(stat(fullPath.c_str(), &entryStat) != 0)
        {
          closedir(dir);
          return -1;
        }

        if(S_ISDIR(entryStat.st_mode))
        {
          int64_t subDirSize = getDirectorySize(fullPath);
          if(subDirSize < 0)
          {
            closedir(dir);
            return -1;
          }
          totalSize += subDirSize;
        }
        else if(S_ISREG(entryStat.st_mode)) { totalSize += static_cast<int64_t>(entryStat.st_size); }
      }

      closedir(dir);
#endif

      return totalSize;
    }
    catch(...)
    {
      return -1;
    }
  }

  /**
   * @brief Захватывает текущий call stack в виде отформатированной строки.
   */
  std::string
  captureStackTrace(int skip_frames = 1, int max_frames = 16) noexcept
  {
    constexpr int kMaxStackFrames = 64;
    std::string result;

    try
    {
#if LOGGER_OS_IS_WINDOWS() && defined(_MSC_VER)
      void *stack[kMaxStackFrames];
      HANDLE process = GetCurrentProcess();

      static std::atomic_bool initialized{false};
      static std::once_flag init_flag;
      static std::mutex dbghelp_mutex;

      if(!initialized.load(std::memory_order_acquire))
      {
        std::call_once(init_flag,
                       [process]()
                       {
                         std::lock_guard<std::mutex> lock(dbghelp_mutex);
                         if(SymInitialize(process, nullptr, TRUE))
                         {
                           SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
                           initialized.store(true, std::memory_order_release);
                         }
                       });
      }

      if(!initialized.load(std::memory_order_acquire))
      {
        WORD frames
          = CaptureStackBackTrace(static_cast<DWORD>(skip_frames), static_cast<DWORD>(max_frames), stack, nullptr);
        for(WORD i = 0; i < frames; ++i)
          result += stringify("  #", i, ": [", formatHex(reinterpret_cast<uintptr_t>(stack[i])), "]\n");
        return result.empty() ? "  Stack trace unavailable (SymInitialize failed)\n" : result;
      }

      WORD frames
        = CaptureStackBackTrace(static_cast<DWORD>(skip_frames), static_cast<DWORD>(max_frames), stack, nullptr);

      if(frames == 0) return "  Stack trace empty (no frames captured)\n";

      constexpr size_t kSymbolBufferSize = sizeof(SYMBOL_INFO) + (MAX_SYM_NAME * sizeof(TCHAR));
      auto *symbol_buffer                = static_cast<SYMBOL_INFO *>(malloc(kSymbolBufferSize));

      if(symbol_buffer == nullptr) return "  Stack trace unavailable (memory allocation failed)\n";

      symbol_buffer->MaxNameLen   = MAX_SYM_NAME;
      symbol_buffer->SizeOfStruct = sizeof(SYMBOL_INFO);

      std::lock_guard<std::mutex> lock(dbghelp_mutex);

      for(WORD i = 0; i < frames; ++i)
      {
        DWORD64 address      = reinterpret_cast<DWORD64>(stack[i]);

        DWORD64 displacement = 0;
        if(SymFromAddr(process, address, &displacement, symbol_buffer) != 0)
        {
          IMAGEHLP_LINE64 line{};
          line.SizeOfStruct       = sizeof(IMAGEHLP_LINE64);
          DWORD line_displacement = 0;

          if(SymGetLineFromAddr64(process, address, &line_displacement, &line) != 0)
          {
            result += stringify("  #", i, ": ", symbol_buffer->Name, " (", line.FileName, ":", line.LineNumber, ") [",
                                formatHex(address), "]\n");
          }
          else { result += stringify("  #", i, ": ", symbol_buffer->Name, " [", formatHex(address), "]\n"); }
        }
        else { result += stringify("  #", i, ": [", formatHex(address), "]\n"); }
      }

      free(symbol_buffer);

#elif defined(__GNUC__) || defined(__clang__)
  #if __has_include(<execinfo.h>)
      void *addresses[kMaxStackFrames];

      int frame_count = backtrace(addresses, kMaxStackFrames);

      if(frame_count <= skip_frames) return "  Stack trace empty (insufficient frames)\n";

      int actual_frames = std::min(frame_count - skip_frames, max_frames);

      char **symbols    = backtrace_symbols(addresses + skip_frames, actual_frames);

      if(!symbols)
      {
        for(int i = 0; i < actual_frames; ++i)
        {
          Dl_info info;
          if(dladdr(addresses[i + skip_frames], &info))
          {
            std::string demangled = info.dli_sname ? info.dli_sname : "??";
    #if __has_include(<cxxabi.h>)
            if(info.dli_sname)
            {
              int status       = -1;
              char *demangled_ = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
              if(status == 0 && demangled_)
              {
                demangled = demangled_;
                free(demangled_);
              }
            }
    #endif
            result += stringify("  #", i, ": ", demangled, " [",
                                formatHex(reinterpret_cast<uintptr_t>(addresses[i + skip_frames])), "]\n");
          }
          else
          {
            result
              += stringify("  #", i, ": [", formatHex(reinterpret_cast<uintptr_t>(addresses[i + skip_frames])), "]\n");
          }
        }
        return result;
      }

      for(int i = 0; i < actual_frames; ++i)
      {
        Dl_info info;
        bool has_symbol = dladdr(addresses[i + skip_frames], &info) && info.dli_sname;
        if(has_symbol)
        {
          std::string demangled = info.dli_sname;
    #if __has_include(<cxxabi.h>)
          int status       = -1;
          char *demangled_ = abi::__cxa_demangle(info.dli_sname, nullptr, nullptr, &status);
          if(status == 0 && demangled_)
          {
            demangled = demangled_;
            free(demangled_);
          }
    #endif

          ptrdiff_t offset
            = reinterpret_cast<char *>(addresses[i + skip_frames]) - reinterpret_cast<char *>(info.dli_saddr);

          result += stringify("  #", i, ": ", demangled, " +", offset, " [",
                              formatHex(reinterpret_cast<uintptr_t>(addresses[i + skip_frames])), "]\n");
        }
        else
        {
          result += stringify("  #", i, ": ", symbols[i], " [",
                              formatHex(reinterpret_cast<uintptr_t>(addresses[i + skip_frames])), "]\n");
        }
      }

      free(symbols);
  #else
      result = "  Stack trace unavailable (<execinfo.h> not available)\n";
      result += stringify("  Current function: ", LOGGER_FUNCTION_NAME, "\n");
  #endif

#else
      result = "  Stack trace not supported on this platform\n";
      result += stringify("  Current function: ", LOGGER_FUNCTION_NAME, "\n");
#endif
    }
    catch(std::exception const &exc)
    {
      return stringify("  Stack trace unavailable (exception: ", exc.what(), ")\n");
    }
    catch(...)
    {
      return "  Stack trace unavailable (unknown exception)\n";
    }

    return result.empty() ? "  Stack trace empty\n" : result;
  }

  /**
   * @brief Преобразует строку в верхний регистр и удаляет пробелы.
   * @param str Исходная строка.
   * @return Нормализованная строка в верхнем регистре без пробелов.
   * @note Не выбрасывает исключений.
   */
  std::string
  _normalizeString(std::string const &str) noexcept
  {
    try
    {
      std::string result = str;

      // Преобразуем в верхний регистр
#if __cplusplus >= 202002L
      std::ranges::transform(result, result.begin(), ::toupper);
#else
      std::transform(result.begin(), result.end(), result.begin(), ::toupper);
#endif

      // Убираем пробелы
#if __cplusplus >= 202002L
      auto iter = std::ranges::remove_if(result, ::isspace);
      result.erase(iter.begin(), result.end());
#else
      result.erase(std::remove_if(result.begin(), result.end(), ::isspace), result.end());
#endif

      return result;
    }
    catch(...)
    {
      // В случае любой ошибки возвращаем исходную строку
      return str;
    }
  }

  /**
   * @brief Создает директорию если она не существует.
   * @param dirPath Путь к директории для создания.
   * @return true, если директория создана или уже существует, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  bool
  _createDirectoryIfNotExists(std::string const &dirPath) noexcept
  {
    try
    {
      if(dirPath.empty()) return false;

#if defined(_WIN32) || defined(_WIN64)
      // Windows: используем CreateDirectoryA
      BOOL result = CreateDirectoryA(dirPath.c_str(), nullptr);
      if(result != 0)
      {
        // Директория создана успешно
        return true;
      }

      // Проверяем причину неудачи
      DWORD error = GetLastError();

      if(error == ERROR_ALREADY_EXISTS)
      {
        // Директория уже существует, проверяем что это действительно директория
        DWORD attributes = GetFileAttributesA(dirPath.c_str());
        bool isDir       = (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0);

        if(isDir)
        {
          // Проверяем права доступа к директории - пытаемся создать тестовый файл
          std::string testFile = dirPath + "\\test_write_access.tmp";
          std::ofstream testStream(testFile, std::ios::out);
          if(testStream.is_open())
          {
            testStream.close();
            std::remove(testFile.c_str());
            return true;
          }

          return false;
        }

        return isDir;
      }

      // Другие ошибки - не удалось создать директорию
      return false;
#else
      // Unix-like системы: используем mkdir
      struct stat st{};
      if(stat(dirPath.c_str(), &st) == 0)
      {
        // Директория уже существует
        return S_ISDIR(st.st_mode);
      }

      // Создаем директорию с правами 0755
      return mkdir(dirPath.c_str(), 0755) == 0;
#endif
    }
    catch(...)
    {
      // В случае любой ошибки возвращаем false
      return false;
    }
  }

  /**
   * @brief Парсит строку конфигурации с префиксом в формате "PREFIX=value".
   * @param line Строка для парсинга.
   * @param prefix Префикс для поиска (например, "LEVEL").
   * @return Значение после знака равенства, или пустую строку если префикс не найден.
   * @note Не выбрасывает исключений.
   */
  std::string
  _parsePrefixedValue(std::string const &line, std::string const &prefix) noexcept
  {
    try
    {
      if(line.empty() || prefix.empty()) return "";

      // Ищем префикс в начале строки (case-insensitive)
      std::string normalizedLine   = _normalizeString(line);
      std::string normalizedPrefix = _normalizeString(prefix);

      if(normalizedLine.length() <= normalizedPrefix.length()) return "";

      // Проверяем, начинается ли строка с префикса
      if(normalizedLine.substr(0, normalizedPrefix.length()) != normalizedPrefix) return "";

      // Проверяем, есть ли знак равенства после префикса
      if(normalizedLine[normalizedPrefix.length()] != '=') return "";

      // Извлекаем значение после знака равенства
      std::string value = line.substr(prefix.length() + 1);

      // Убираем пробелы в начале и конце значения
      value.erase(0, value.find_first_not_of(" \t\r\n"));
      value.erase(value.find_last_not_of(" \t\r\n") + 1);

      return value;
    }
    catch(...)
    {
      return "";
    }
  }

  /**
   * @brief Проверяет, является ли строка строкой с префиксом.
   * @param line Строка для проверки.
   * @param prefix Префикс для поиска.
   * @return true, если строка содержит префикс, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  bool
  _hasPrefix(std::string const &line, std::string const &prefix) noexcept
  {
    try
    {
      return !_parsePrefixedValue(line, prefix).empty();
    }
    catch(...)
    {
      return false;
    }
  }

  /**
   * @brief Возвращает содержимое файла-хелпера с подсказкой для разработчиков.
   * @return Строка с содержимым подсказки (bilingual: Russian and English).
   */
  std::string
  getHintFileContent()
  {
    std::stringstream content;

    content << "\n";
    content << "=============================================================\n";
    content << "  Universal Logger - Подсказка для разработчиков / Hint for Developers\n";
    content << "=============================================================\n";
    content << "\n";

    // ========== RUSSIAN SECTION ==========
    content << "[RU] ВАЖНО: Создайте файл 'enable_logs' рядом с исполняемым файлом\n";
    content << "[RU]        (.exe/.so/.dll/.dylib) для включения логирования.\n";
    content << "\n";
    content << "[RU] Параметры конфигурации (case-insensitive, порядок не важен):\n";
    content << "\n";
    content << "[RU]   LEVEL=<уровень>          - Уровень логирования\n";
    content << "[RU]                            Значения: trace, debug, info, warning, error, fatal\n";
    content << "\n";
    content << "[RU]   FUNCNAME=<режим>        - Режим отображения имени функции\n";
    content << "[RU]                            Значения: none, short, signature\n";
    content << "[RU]                            - none: не показывать имя функции\n";
    content << "[RU]                            - short: короткое имя (__func__)\n";
    content << "[RU]                            - signature: полная сигнатура (__FUNCSIG__/__PRETTY_FUNCTION__)\n";
    content << "\n";
    content << "[RU]   TIMESTAMPED=<значение>   - Использовать timestamped логи\n";
    content << "[RU]                            Значения: true/yes/1 или false/no/0\n";
    content << "\n";
    content << "[RU]   STACKTRACE=<значение>    - Показывать stacktrace (по умолчанию true для WARNING/ERROR)\n";
    content << "[RU]                            Значения: true/yes/1 или false/no/0\n";
    content << "[RU]                            ВАЖНО: Если STACKTRACE=true, то stacktrace записывается\n";
    content << "[RU]                            под абсолютно каждую функцию для ВСЕХ уровней логирования!\n";
    content << "\n";
    content << "[RU]   STACKTRACE_FRAMES=<num> - Максимальное количество фреймов в stacktrace\n";
    content << "[RU]                            Значение: число от 1 до 64 (по умолчанию 16)\n";
    content << "\n";
    content << "[RU]   HINT=<значение>          - Создавать ли файл-хелпер с подсказкой\n";
    content << "[RU]                            Значения: true/yes/1 (создавать) или false/no/0 (не создавать)\n";
    content << "\n";
    content << "[RU]   PRESET=<компоненты>      - Фильтрация логов по компонентам\n";
    content << "[RU]                            Значение: список компонентов через запятую (например, "
               "NetworkManager,DatabaseConnection)\n";
    content << "[RU]                            Формат компонента в логах: [ComponentName]: message\n";
    content << "[RU]                            Если PRESET задан, логируются только указанные компоненты\n";
    content << "\n";
    content << "[RU]   BUFFERING=<значение>     - Включить/отключить буферизацию логов\n";
    content
      << "[RU]                            Значения: true/yes/1 (включить) или false/no/0 (отключить, по умолчанию)\n";
    content << "[RU]                            При включенной буферизации логи сохраняются в буфере\n";
    content << "[RU]                            и записываются при WARNING/ERROR/FATAL или при вызове flush()\n";
    content << "\n";
    content << "[RU]   BUFFER_SIZE=<число>      - Размер буфера логов (количество записей)\n";
    content << "[RU]                            Значение: положительное число (по умолчанию 100)\n";
    content << "[RU]                            Используется только при BUFFERING=true\n";
    content << "\n";
    content << "[RU] Примеры конфигурации:\n";
    content << "\n";
    content << "[RU] 1. Минимальная конфигурация (только уровень):\n";
    content << "   LEVEL=info\n";
    content << "\n";
    content << "[RU] 2. Полная конфигурация:\n";
    content << "   LEVEL=debug\n";
    content << "   FUNCNAME=short\n";
    content << "   TIMESTAMPED=true\n";
    content << "   STACKTRACE=true\n";
    content << "   STACKTRACE_FRAMES=10\n";
    content << "\n";
    content << "[RU] 3. Пример с произвольным порядком строк:\n";
    content << "   TIMESTAMPED=true\n";
    content << "   FUNCNAME=signature\n";
    content << "   LEVEL=warning\n";
    content << "   STACKTRACE=false\n";
    content << "\n";
    content << "[RU] 4. Отключение имен функций:\n";
    content << "   LEVEL=trace\n";
    content << "   FUNCNAME=none\n";
    content << "\n";
    content << "[RU] 5. Отключение файла-хелпера:\n";
    content << "   LEVEL=info\n";
    content << "   HINT=false\n";
    content << "\n";
    content << "[RU] 6. Включение буферизации с настраиваемым размером:\n";
    content << "   LEVEL=debug\n";
    content << "   BUFFERING=true\n";
    content << "   BUFFER_SIZE=200\n";
    content << "\n";
    content << "[RU] 7. Пресет компонентов с буферизацией:\n";
    content << "   LEVEL=info\n";
    content << "   PRESET=NetworkManager,DatabaseConnection\n";
    content << "   BUFFERING=true\n";
    content << "   BUFFER_SIZE=150\n";
    content << "\n";
    content << "[RU] ПРИМЕЧАНИЕ: Все параметры case-insensitive (регистр не важен).\n";
    content << "[RU]            Можно писать: LEVEL=INFO, level=info, LeVeL=InFo - все равно работает.\n";
    content << "[RU]            Порядок строк в файле не важен.\n";
    content << "\n\n\n";

    // ========== ENGLISH SECTION ==========
    content << "[EN] IMPORTANT: Create 'enable_logs' file next to the executable\n";
    content << "[EN]            (.exe/.so/.dll/.dylib) to enable logging.\n";
    content << "\n";
    content << "[EN] Configuration parameters (case-insensitive, order doesn't matter):\n";
    content << "\n";
    content << "[EN]   LEVEL=<level>            - Logging level\n";
    content << "[EN]                            Values: trace, debug, info, warning, error, fatal\n";
    content << "\n";
    content << "[EN]   FUNCNAME=<mode>         - Function name display mode\n";
    content << "[EN]                            Values: none, short, signature\n";
    content << "[EN]                            - none: don't show function name\n";
    content << "[EN]                            - short: short name (__func__)\n";
    content << "[EN]                            - signature: full signature (__FUNCSIG__/__PRETTY_FUNCTION__)\n";
    content << "\n";
    content << "[EN]   TIMESTAMPED=<value>      - Use timestamped logs\n";
    content << "[EN]                            Values: true/yes/1 or false/no/0\n";
    content << "\n";
    content << "[EN]   STACKTRACE=<value>       - Show stacktrace (default: true for WARNING/ERROR)\n";
    content << "[EN]                            Values: true/yes/1 or false/no/0\n";
    content << "[EN]                            IMPORTANT: If STACKTRACE=true, stacktrace is written\n";
    content << "[EN]                            under EVERY function for ALL log levels!\n";
    content << "\n";
    content << "[EN]   STACKTRACE_FRAMES=<num> - Maximum number of frames in stacktrace\n";
    content << "[EN]                            Value: number from 1 to 64 (default: 16)\n";
    content << "\n";
    content << "[EN]   HINT=<value>             - Create hint file for developers\n";
    content << "[EN]                            Values: true/yes/1 (create) or false/no/0 (don't create)\n";
    content << "\n";
    content << "[EN]   PRESET=<components>      - Filter logs by components\n";
    content << "[EN]                            Value: comma-separated list of components (e.g., "
               "NetworkManager,DatabaseConnection)\n";
    content << "[EN]                            Component format in logs: [ComponentName]: message\n";
    content << "[EN]                            If PRESET is set, only specified components are logged\n";
    content << "\n";
    content << "[EN]   BUFFERING=<value>        - Enable/disable log buffering\n";
    content << "[EN]                            Values: true/yes/1 (enable) or false/no/0 (disable, default)\n";
    content << "[EN]                            When enabled, logs are stored in buffer and written\n";
    content << "[EN]                            on WARNING/ERROR/FATAL or when flush() is called\n";
    content << "\n";
    content << "[EN]   BUFFER_SIZE=<number>     - Log buffer size (number of entries)\n";
    content << "[EN]                            Value: positive number (default: 100)\n";
    content << "[EN]                            Used only when BUFFERING=true\n";
    content << "\n";
    content << "[EN] Configuration examples:\n";
    content << "\n";
    content << "[EN] 1. Minimal configuration (level only):\n";
    content << "   LEVEL=info\n";
    content << "\n";
    content << "[EN] 2. Full configuration:\n";
    content << "   LEVEL=debug\n";
    content << "   FUNCNAME=short\n";
    content << "   TIMESTAMPED=true\n";
    content << "   STACKTRACE=true\n";
    content << "   STACKTRACE_FRAMES=10\n";
    content << "\n";
    content << "[EN] 3. Example with arbitrary line order:\n";
    content << "   TIMESTAMPED=true\n";
    content << "   FUNCNAME=signature\n";
    content << "   LEVEL=warning\n";
    content << "   STACKTRACE=false\n";
    content << "\n";
    content << "[EN] 4. Disabling function names:\n";
    content << "   LEVEL=trace\n";
    content << "   FUNCNAME=none\n";
    content << "\n";
    content << "[EN] 5. Disabling hint file:\n";
    content << "   LEVEL=info\n";
    content << "   HINT=false\n";
    content << "\n";
    content << "[EN] 6. Enabling buffering with custom size:\n";
    content << "   LEVEL=debug\n";
    content << "   BUFFERING=true\n";
    content << "   BUFFER_SIZE=200\n";
    content << "\n";
    content << "[EN] 7. Component preset with buffering:\n";
    content << "   LEVEL=info\n";
    content << "   PRESET=NetworkManager,DatabaseConnection\n";
    content << "   BUFFERING=true\n";
    content << "   BUFFER_SIZE=150\n";
    content << "\n";
    content << "[EN] NOTE: All parameters are case-insensitive.\n";
    content << "[EN]            You can write: LEVEL=INFO, level=info, LeVeL=InFo - all work the same.\n";
    content << "[EN]            Line order in the file doesn't matter.\n";
    content << "\n";
    content << "=============================================================\n";
    content << "\n";

    return content.str();
  }

  /**
   * @brief Выводит подсказку разработчикам о том, как использовать логгер.
   * @note Выводит информацию в stdout о создании файла-триггера и примерах конфигурации.
   *       Bilingual output: Russian and English.
   */
  void
  fileHintAboutLogger()
  {
    std::cout << getHintFileContent();
  }
} // anonymous namespaces for utility functions

Logger::Logger()
    : m_currentLogLevel(LogLevel::LEVEL_INFO),
      m_loggingEnabled(false),
      m_functionNameMode(FunctionNameMode::FULL),
      m_useTimestampedLogs(false),
      m_showStackTrace(false),
      m_stackTraceMaxFrames(logger_config_t::kMaxStackTraceFrames),
      m_hintShown(false),
      m_triggerFileName(kDefaultTriggerFileName),
      m_bufferingEnabled(false),
      m_bufferSize(logger_config_t::kBufferSize),
      m_presetEnabled(false)
{
  try
  {
    m_executableDirectory = _getExecutableDirectory();

    // Проверяем наличие файла включения логирования
    m_loggingEnabled = _isLoggingEnabledFileExists();
    if(!m_loggingEnabled)
    {
      // Логирование отключено, не создаем файл лога
      return;
    }

    // Читаем конфигурацию из файла (уровень логирования, режим имени функции, timestamped logs, stacktrace, preset)
    auto config           = _readConfigFromFile();
    m_currentLogLevel     = config.m_logLevel;
    m_functionNameMode    = config.m_funcNameMode;
    m_useTimestampedLogs  = config.m_useTimestampedLogs;
    m_showStackTrace      = config.m_showStackTrace;
    m_stackTraceMaxFrames = config.m_stackTraceMaxFrames;

    // Применяем пресет из конфигурации, если он задан
    if(!config.m_presetComponents.empty())
    {
      m_presetEnabled    = true;
      m_presetComponents = config.m_presetComponents;
    }

    // Применяем настройки буферизации из конфигурации
    if(config.m_bufferingEnabled) { enableBuffering(logger_config_t::kBufferSize); }
    else
    {
      // Буферизация отключена (по умолчанию)
      m_bufferingEnabled = false;
      m_bufferSize       = logger_config_t::kBufferSize; // Сохраняем размер на случай включения программно
    }

    // Управление файлом-хелпером
    std::string hintFilePath = m_executableDirectory;
    if(!hintFilePath.empty() && hintFilePath[hintFilePath.length() - 1] != LOGGER_FILE_SEPARATOR)
      hintFilePath += LOGGER_FILE_SEPARATOR;
    hintFilePath += kHintFileName;

    if(config.m_createHintFile)
    {
      // Создаем или обновляем файл-хелпер
      try
      {
        std::ofstream hintFile(hintFilePath, std::ios::out | std::ios::trunc);
        if(hintFile.is_open())
        {
          hintFile << getHintFileContent();
          hintFile.close();
        }
      }
      catch(...) // NOLINT(bugprone-empty-catch)
      {
        // Игнорируем ошибки создания файла-хелпера
      }
    }
    else
    {
      // Удаляем файл-хелпер, если он существует
      try
      {
        std::ifstream testFile(hintFilePath);
        if(testFile.good())
        {
          testFile.close();
          std::remove(hintFilePath.c_str()); // NOLINT(cppcoreguidelines-owning-memory)
        }
      }
      catch(...) // NOLINT(bugprone-empty-catch)
      {
        // Игнорируем ошибки удаления файла-хелпера
      }
    }

    m_logFilePath = _createLogFilePath();

    // Открываем файл для записи, перезаписывая содержимое
    m_logFile.open(m_logFilePath, std::ios::out | std::ios::trunc);
    if(!m_logFile.is_open())
    {
      std::string errorMsg = "Cannot open log file: ";
      errorMsg.reserve(errorMsg.length() + m_logFilePath.length());
      errorMsg += m_logFilePath;
      throw std::runtime_error(errorMsg);
    }

    // Записываем заголовок лога
    m_logFile << "=== Log start of the program ===\n";
    m_logFileStartTime = std::chrono::system_clock::now();
    m_logFile << "Time start: " << _formatTimestamp() << '\n';
    m_logFile << "Path to executable file: " << m_executableDirectory << '\n';
    m_logFile << "Logging enabled by file: " << m_triggerFileName << '\n';
    m_logFile << "Log level from trigger-file: " << LogLevelToString(m_currentLogLevel) << '\n';
    std::string funcModeStr = "NONE";
    if(m_functionNameMode == FunctionNameMode::SHORT)
      funcModeStr = "SHORT";
    else if(m_functionNameMode == FunctionNameMode::FULL)
      funcModeStr = "FULL";
    m_logFile << "Function name mode: " << funcModeStr << '\n';
    m_logFile << "Show stacktrace: " << (m_showStackTrace ? "YES" : "NO") << '\n';
    m_logFile << "Stacktrace max frames: " << static_cast<int>(m_stackTraceMaxFrames) << '\n';
    m_logFile << "Buffering enabled: " << (m_bufferingEnabled ? "YES" : "NO") << '\n';
    if(m_bufferingEnabled) m_logFile << "Buffer size: " << static_cast<int>(m_bufferSize) << " entries\n";
    m_logFile << "Preset enabled: " << (m_presetEnabled ? "YES" : "NO") << '\n';
    if(m_presetEnabled && !m_presetComponents.empty())
    {
      m_logFile << "Preset components: ";
      bool first = true;
      for(auto const &component : m_presetComponents)
      {
        if(!first) m_logFile << ", ";
        m_logFile << component;
        first = false;
      }
      m_logFile << '\n';
    }
    m_logFile << "==================================\n";
    m_logFile.flush();
  }
  catch(std::exception const &e)
  {
    // Если не удалось создать логгер, выводим ошибку в stderr
    std::cerr << "Logger initialization error: " << e.what() << '\n';
    throw;
  }
}

Logger::~Logger()
{
  if(m_loggingEnabled && m_logFile.is_open())
  {
    try
    {
      std::lock_guard<std::mutex> lock(m_logMutex);

      // Если буферизация включена, записываем оставшиеся записи из буфера
      if(m_bufferingEnabled && !m_logBuffer.empty()) _flushBuffer();

      m_logFile << "=== End of the program ============" << '\n';
      m_logFileEndTime = std::chrono::system_clock::now();
      m_logFile << "Time end: " << _formatTimestamp() << '\n';

      auto activeTime    = m_logFileEndTime - m_logFileStartTime;
      auto activeSeconds = std::chrono::duration_cast<std::chrono::seconds>(activeTime);
      auto activeMs      = std::chrono::duration_cast<std::chrono::milliseconds>(activeTime) % std::chrono::seconds(1);
      m_logFile << "Active time: " << activeSeconds.count() << "." << std::setfill('0') << std::setw(3)
                << activeMs.count() << " seconds\n";
      m_logFile << "===================================" << '\n';
      m_logFile.flush();
      m_logFile.close();
    }
    catch(std::exception const &e)
    {
      std::cerr << "Warning: Logger cleanup error: " << e.what() << '\n';
    }
    catch(...)
    {
      std::cerr << "Warning: Unknown error during logger cleanup\n";
    }
  }
}

Logger &
Logger::getInstance()
{
  static Logger instance;
  return instance;
}

void
Logger::setLogLevel(LogLevel level) noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  m_currentLogLevel = level;
}

LogLevel
Logger::getLogLevel() const noexcept
{
  return m_currentLogLevel;
}

void
Logger::log(LogLevel level, std::string const &message)
{
  if(!m_loggingEnabled)
  {
    // Показываем подсказку один раз, если логирование отключено, но кто-то пытается логировать
    if(!m_hintShown)
    {
      m_hintShown = true;
      printLoggerHint();
    }
    return;
  }

  std::lock_guard<std::mutex> lock(m_logMutex);
  if(static_cast<uint8_t>(level) >= static_cast<uint8_t>(m_currentLogLevel))
  {
    // Проверяем пресет перед обработкой сообщения
    if(!_shouldLogByPreset(message)) return;

    // Формируем полное сообщение с учетом stacktrace
    std::string fullMessage = message;

    // Определяем, нужно ли показывать stacktrace
    // По умолчанию stacktrace включен для WARNING и ERROR уровней
    bool shouldShowTrace = m_showStackTrace;
    if(!shouldShowTrace && (level == LogLevel::LEVEL_WARNING || level == LogLevel::LEVEL_ERROR))
    {
      // Если stacktrace глобально отключен, но это WARNING или ERROR - включаем для этих уровней
      shouldShowTrace = true;
    }

    if(shouldShowTrace)
    {
      try
      {
        std::string stackTrace = captureStackTrace(1, m_stackTraceMaxFrames);
        if(!stackTrace.empty()) fullMessage += "\n" + stackTrace;
      }
      catch(std::exception const &exc)
      {
        std::cerr << "Warning: Error during capture stacktrace: " << exc.what() << '\n';
      }
      catch(...)
      {
        std::cerr << "Warning: Unknown error during capture stacktrace\n";
      }
    }

    // Определяем, является ли уровень критическим (WARNING, ERROR, FATAL)
    bool isCriticalLevel
      = (level == LogLevel::LEVEL_WARNING || level == LogLevel::LEVEL_ERROR || level == LogLevel::LEVEL_FATAL);

    // Если буферизация включена и уровень не критический - добавляем в буфер
    if(m_bufferingEnabled && !isCriticalLevel)
    {
      // Добавляем запись в буфер
      m_logBuffer.emplace_back(level, fullMessage);

      // Если буфер превысил размер, удаляем самые старые записи
      if(m_logBuffer.size() > m_bufferSize) m_logBuffer.pop_front();

      return; // Не записываем сразу, только в буфер
    }

    // Если буферизация включена и уровень критический - записываем буфер, затем текущее сообщение
    if(m_bufferingEnabled && isCriticalLevel)
    {
      // Записываем все записи из буфера
      if(!m_logBuffer.empty()) _flushBuffer();
    }

    // Проверяем, можно ли записать лог без превышения лимитов
    if(_canWriteLog(fullMessage.length())) { _writeLog(level, fullMessage); }
    else
    {
      // Если не можем записать из-за превышения лимитов, записываем предупреждение
      std::string warningMsg
        = "LOG SIZE LIMIT EXCEEDED: Cannot write log message. Current size: "
          + _formatSize(m_useTimestampedLogs
                          ? getDirectorySize(m_executableDirectory + LOGGER_FILE_SEPARATOR + kLogDirectoryName)
                          : getFileSize(m_logFilePath))
          + ", Free space: " + _formatSize(getFreeDiskSpace(m_logFilePath));
      _writeLog(LogLevel::LEVEL_WARNING, warningMsg);
    }
  }
}

bool
Logger::isLevelEnabled(LogLevel level) const noexcept
{
  if(!m_loggingEnabled) return false;
  return static_cast<uint8_t>(level) >= static_cast<uint8_t>(m_currentLogLevel);
}

bool
Logger::isLoggingEnabled() const noexcept
{
  return m_loggingEnabled;
}

FunctionNameMode
Logger::getFunctionNameMode() const noexcept
{
  return m_functionNameMode;
}

bool
Logger::shouldShowFunctionName() const noexcept
{
  return m_functionNameMode != FunctionNameMode::NONE;
}

std::string
Logger::getLogFilePath() const noexcept
{
  return m_logFilePath;
}

void
Logger::printLoggerHint() // NOLINT(readability-convert-member-functions-to-static)
  const noexcept
{
  try
  {
    fileHintAboutLogger();
  }
  catch(...) // NOLINT(bugprone-empty-catch)
  {
    // Игнорируем ошибки при выводе подсказки
  }
}

void
Logger::flush()
{
  if(!m_loggingEnabled) return;

  std::lock_guard<std::mutex> lock(m_logMutex);
  if(m_logFile.is_open()) m_logFile.flush();
}

void
Logger::enableBuffering(size_t kBufferSize) noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  m_bufferingEnabled = true;
  m_bufferSize       = (kBufferSize > 0)
                         ? kBufferSize
                         : logger_config_t::kBufferSize; // Минимум 1, по умолчанию logger_config_t::kBufferSize (100)

  // Если новый размер меньше текущего размера буфера, удаляем старые записи
  if(m_logBuffer.size() > m_bufferSize)
  {
    size_t elementsToRemove = m_logBuffer.size() - m_bufferSize;
    m_logBuffer.erase(m_logBuffer.begin(), m_logBuffer.begin() + static_cast<ptrdiff_t>(elementsToRemove));
  }
}

void
Logger::disableBuffering()
{
  std::lock_guard<std::mutex> lock(m_logMutex);

  // Записываем все записи из буфера перед отключением
  if(!m_logBuffer.empty()) _flushBuffer();

  m_bufferingEnabled = false;
  m_logBuffer.clear();
}

void
Logger::setBufferSize(size_t bufferSize) noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  m_bufferSize = (bufferSize > 0)
                   ? bufferSize
                   : logger_config_t::kBufferSize; // Минимум 1, по умолчанию logger_config_t::kBufferSize (100)

  // Если новый размер меньше текущего размера буфера, удаляем старые записи
  if(m_logBuffer.size() > m_bufferSize)
  {
    size_t elementsToRemove = m_logBuffer.size() - m_bufferSize;
    m_logBuffer.erase(m_logBuffer.begin(), m_logBuffer.begin() + static_cast<ptrdiff_t>(elementsToRemove));
  }
}

bool
Logger::isBufferingEnabled() const noexcept
{
  return m_bufferingEnabled;
}

size_t
Logger::getBufferSize() const noexcept
{
  return m_bufferSize;
}

void
Logger::enablePreset(std::unordered_set<std::string> const &components) noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  m_presetEnabled    = true;
  m_presetComponents = components;
}

void
Logger::disablePreset() noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  m_presetEnabled = false;
  m_presetComponents.clear();
}

void
Logger::setPresetComponents(std::unordered_set<std::string> const &components) noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  m_presetEnabled    = true; // Автоматически включаем пресет
  m_presetComponents = components;
}

bool
Logger::isPresetEnabled() const noexcept
{
  return m_presetEnabled;
}

std::unordered_set<std::string>
Logger::getPresetComponents() const noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  return m_presetComponents; // Возвращаем копию
}

void
Logger::setTriggerFileName(std::string const &fileName) noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  if(!fileName.empty()) m_triggerFileName = fileName;
}

std::string
Logger::getTriggerFileName() const noexcept
{
  std::lock_guard<std::mutex> lock(m_logMutex);
  return m_triggerFileName;
}

std::unordered_set<std::string>
Logger::_parsePresetComponents(std::string const &componentsStr) noexcept
{
  std::unordered_set<std::string> result;
  try
  {
    if(componentsStr.empty()) return result;

    std::stringstream ss(componentsStr);
    std::string component;

    // Разделяем строку по запятым
    while(std::getline(ss, component, ','))
    {
      // Убираем пробелы в начале и конце
      component.erase(0, component.find_first_not_of(" \t\r\n"));
      component.erase(component.find_last_not_of(" \t\r\n") + 1);

      // Добавляем компонент, если он не пустой
      if(!component.empty()) result.insert(component);
    }
  }
  catch(...)
  {
    // В случае ошибки возвращаем пустой набор
    return {};
  }

  return result;
}

// NOLINTBEGIN(cppcoreguidelines-avoid-c-arrays, cppcoreguidelines-pro-bounds-array-to-pointer-decay,
// cppcoreguidelines-pro-type-reinterpret-cast)

std::string
Logger::_getExecutableDirectory() const
{
#if defined(_WIN32) || defined(_WIN64)
  // Windows: используем GetModuleFileName для получения пути к DLL/EXE
  char path[LOGGER_MAX_PATH];
  HMODULE hModule = nullptr;

  // Пытаемся получить handle текущего модуля
  if(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                        reinterpret_cast<LPCSTR>(this), &hModule))
  {
    if(GetModuleFileNameA(hModule, path, LOGGER_MAX_PATH) > 0) return _extractDirectoryFromPath(path);
  }

  // Fallback: используем GetModuleFileName с nullptr
  if(GetModuleFileNameA(nullptr, path, LOGGER_MAX_PATH) > 0) return _extractDirectoryFromPath(path);

  throw std::runtime_error("Can't get executable directory on Windows");

#elif defined(__linux__)
  // Linux: читаем /proc/self/exe для получения пути к исполняемому файлу
  char path[LOGGER_MAX_PATH];
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if(len != -1)
  {
    path[len] = '\0';
    return _extractDirectoryFromPath(path);
  }

  // Fallback: используем getcwd
  char cwd[LOGGER_MAX_PATH];
  if(getcwd(cwd, sizeof(cwd)) != nullptr) return std::string(cwd);

  throw std::runtime_error("Can't get executable directory on Linux");

#elif defined(__APPLE__)
  // macOS: используем _NSGetExecutablePath
  char path[LOGGER_MAX_PATH];
  std::uint32_t size = sizeof(path);
  if(_NSGetExecutablePath(path, &size) == 0) return _extractDirectoryFromPath(path);

  // Fallback: используем getcwd
  char cwd[LOGGER_MAX_PATH];
  if(getcwd(cwd, sizeof(cwd)) != nullptr) return std::string(cwd);

  throw std::runtime_error("Can't get executable directory on macOS");

#else
  // Неизвестная платформа: используем текущую рабочую директорию
  char cwd[LOGGER_MAX_PATH];
  if(getcwd(cwd, sizeof(cwd)) != nullptr) return std::string(cwd);

  throw std::runtime_error("Unknown platform, can't get executable directory");
#endif
}

// NOLINTEND(cppcoreguidelines-avoid-c-arrays, cppcoreguidelines-pro-bounds-array-to-pointer-decay,
// cppcoreguidelines-pro-type-reinterpret-cast)

std::string
Logger::_createLogFilePath() const
{
  if(m_useTimestampedLogs) return _createTimestampedLogFilePath();
  return _createSingleLogFilePath();
}

std::string
Logger::_createSingleLogFilePath() const
{
  std::string logPath = m_executableDirectory;

  // Добавляем разделитель пути если его нет
  if(!logPath.empty() && logPath[logPath.length() - 1] != LOGGER_FILE_SEPARATOR) logPath += LOGGER_FILE_SEPARATOR;

  logPath += kDefaultLogFileName;
  return logPath;
}

std::string
Logger::_createTimestampedLogFilePath() const
{
  std::string logDir = m_executableDirectory;

  // Добавляем разделитель пути если его нет
  if(!logDir.empty() && logDir[logDir.length() - 1] != LOGGER_FILE_SEPARATOR) logDir += LOGGER_FILE_SEPARATOR;

  // Добавляем имя директории для timestamped логов
  logDir += kLogDirectoryName;

  // Создаем директорию если она не существует
  if(!_createDirectoryIfNotExists(logDir))
  {
    // Если не удалось создать директорию, возвращаем путь к обычному файлу лога
    // Это fallback для случаев когда директория не может быть создана
    return _createSingleLogFilePath();
  }

  // Добавляем разделитель пути к директории
  logDir += LOGGER_FILE_SEPARATOR;

  // Генерируем имя файла с timestamp
  std::string timestamp = _formatTimestampForFilename();
  std::string filename  = "logger_" + timestamp + ".log";

  std::string fullPath  = logDir + filename;

  // Дополнительная проверка: пытаемся создать тестовый файл для проверки прав доступа
  std::ofstream testFile(fullPath, std::ios::out | std::ios::app);
  if(!testFile.is_open())
  {
    // Если не можем создать файл в timestamped директории, fallback к обычному файлу
    testFile.close();
    return _createSingleLogFilePath();
  }
  testFile.close();

  // Удаляем тестовый файл
  std::remove(fullPath.c_str());

  return fullPath;
}

std::string
Logger::_formatTimestampForFilename()
{
  auto now      = std::chrono::system_clock::now();
  auto raw_time = std::chrono::system_clock::to_time_t(now);

  std::stringstream sstream;

  // Thread-safe localtime: localtime_s (Win) / localtime_r (POSIX)
  struct tm timeinfo{};
#if defined(_WIN32) || defined(_WIN64)
  localtime_s(std::addressof(timeinfo), std::addressof(raw_time));
#else
  localtime_r(std::addressof(raw_time), std::addressof(timeinfo)); // POSIX потокобезопасная версия
#endif

  // Форматируем в формате DD.MM.YYYY-hh-mm-ss (без двоеточий для Windows)
  constexpr int YEAR_OFFSET = 1900;
  sstream << std::setfill('0') << std::setw(2) << timeinfo.tm_mday << "." << std::setfill('0') << std::setw(2)
          << (timeinfo.tm_mon + 1) << "." << std::setfill('0') << std::setw(4) << (timeinfo.tm_year + YEAR_OFFSET)
          << "-" << std::setfill('0') << std::setw(2) << timeinfo.tm_hour << "-" << std::setfill('0') << std::setw(2)
          << timeinfo.tm_min << "-" << std::setfill('0') << std::setw(2) << timeinfo.tm_sec;

  return sstream.str();
}

std::string
Logger::_formatTimestamp()
{
  auto now = std::chrono::system_clock::now();
  // Здесь нужно переименовать переменную, чтобы избежать затенения глобального типа time_t из <time.h>
  // ISO C++: Локальные имена могут затенять глобальные имена, но -Wshadow warns for clarity
  auto raw_time = std::chrono::system_clock::to_time_t(now);

  auto ms_count = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch())
                  % std::chrono::milliseconds::period::den;

  std::stringstream sstream;

  // Thread-safe localtime: localtime_s (Win) / localtime_r (POSIX)
  // ISO C: localtime() uses static buffer - race condition in multi-threaded code (гонка в многопоточном коде)
  struct tm timeinfo{};
#if defined(_WIN32) || defined(_WIN64)
  localtime_s(std::addressof(timeinfo), std::addressof(raw_time));
#else
  localtime_r(std::addressof(raw_time), std::addressof(timeinfo)); // POSIX потокобезопасная версия
#endif
  sstream << std::put_time(std::addressof(timeinfo), "%Y-%m-%d %H:%M:%S");
  sstream << '.' << std::setfill('0') << std::setw(3) << ms_count.count();

  return sstream.str();
}

std::string
Logger::_levelToString(LogLevel level)
{
  switch(level)
  {
  case LogLevel::LEVEL_TRACE: return "TRACE";
  case LogLevel::LEVEL_DEBUG: return "DEBUG";
  case LogLevel::LEVEL_INFO: return "INFO ";
  case LogLevel::LEVEL_SUCCESS: return "SUCCESS";
  case LogLevel::LEVEL_WARNING: return "WARNING";
  case LogLevel::LEVEL_ERROR: return "ERROR";
  case LogLevel::LEVEL_FATAL: return "FATAL";
  default: return "UNKNOWN";
  }
}

void
Logger::_writeLog(LogLevel level, std::string const &message)
{
  // Lock already held by caller

  if(m_logFile.is_open())
  {
    try
    {
      m_logFile << "[" << _formatTimestamp() << "] "
                << "[" << _levelToString(level) << "] " << message << '\n';

      // Делаем flush только для критических уровней, потому что:
      // 1. ERROR и FATAL сообщения критически важны и должны быть записаны немедленно
      // 2. Постоянный flush() после каждого лога значительно снижает производительность
      // 3. Для обычных уровней (TRACE, DEBUG, INFO, WARNING) достаточно буферизации
      // 4. Операционная система сама сбросит буферы в файл при необходимости
      // 5. Принудительный flush() только для критических уровней обеспечивает баланс
      //    между производительностью и надежностью записи важных сообщений
      if(level >= LogLevel::LEVEL_ERROR) m_logFile.flush();
    }
    catch(std::ios_base::failure const &e)
    {
      // Если не удалось записать в файл, выводим в stderr
      std::cerr << "Error writing to log: " << e.what() << '\n';
      throw;
    }
  }
}

std::string
Logger::_extractDirectoryFromPath(std::string const &fullPath)
{
  // Проверяем на пустую строку
  if(fullPath.empty()) return ".";

  std::string directory = fullPath;

  // Ищем последний разделитель пути
  size_t lastSlash = directory.find_last_of(LOGGER_FILE_SEPARATOR);
  if(lastSlash != std::string::npos && lastSlash < directory.length()) { directory = directory.substr(0, lastSlash); }
  else
  {
    // Если разделителей нет, возвращаем текущую директорию
    directory = ".";
  }

  return directory;
}

bool
Logger::_isLoggingEnabledFileExists() const noexcept
{
  try
  {
    std::string enableFilePath = m_executableDirectory;

    // Добавляем разделитель пути если его нет
    if(!enableFilePath.empty() && enableFilePath[enableFilePath.length() - 1] != LOGGER_FILE_SEPARATOR)
      enableFilePath += LOGGER_FILE_SEPARATOR;

    enableFilePath += m_triggerFileName;

    // Проверяем существование файла
#if defined(_WIN32) || defined(_WIN64)
    DWORD attributes = GetFileAttributesA(enableFilePath.c_str());
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);
#else
    // Unix-like системы: используем access()
    return (access(enableFilePath.c_str(), F_OK) == 0);
#endif
  }
  catch(...)
  {
    // В случае любой ошибки считаем, что логирование отключено
    return false;
  }
}

logger_config_t
Logger::_readConfigFromFile() // NOLINT(readability-function-cognitive-complexity)
  const noexcept
{
  try
  {
    std::string enableFilePath = m_executableDirectory;

    // Добавляем разделитель пути если его нет
    if(!enableFilePath.empty() && enableFilePath[enableFilePath.length() - 1] != LOGGER_FILE_SEPARATOR)
      enableFilePath += LOGGER_FILE_SEPARATOR;

    enableFilePath += m_triggerFileName;

    // Открываем файл для чтения
    std::ifstream file(enableFilePath);
    if(!file.is_open()) return logger_config_t{}; // Используем значения по умолчанию

    logger_config_t config;

    std::string line;
    // Читаем все строки файла и парсим их независимо от порядка
    while(std::getline(file, line))
    {
      // Убираем пробелы в начале и конце
      line.erase(0, line.find_first_not_of(" \t\r\n"));
      line.erase(line.find_last_not_of(" \t\r\n") + 1);

      // Пропускаем пустые строки и комментарии
      if(line.empty() || line[0] == '#') continue;

      // Парсим строки с префиксами (case-insensitive)
      if(_hasPrefix(line, "LEVEL"))
      {
        std::string value = _parsePrefixedValue(line, "LEVEL");
        if(!value.empty()) config.m_logLevel = _stringToLogLevel(value);
      }
      else if(_hasPrefix(line, "FUNCNAME"))
      {
        std::string value = _parsePrefixedValue(line, "FUNCNAME");
        if(!value.empty()) config.m_funcNameMode = _parseFunctionNameMode(value);
      }
      else if(_hasPrefix(line, "TIMESTAMPED"))
      {
        std::string value = _parsePrefixedValue(line, "TIMESTAMPED");
        if(!value.empty()) config.m_useTimestampedLogs = _shouldUseTimestampedLogs(value);
      }
      else if(_hasPrefix(line, "STACKTRACE"))
      {
        std::string value = _parsePrefixedValue(line, "STACKTRACE");
        if(!value.empty()) config.m_showStackTrace = _shouldShowStackTrace(value);
      }
      else if(_hasPrefix(line, "STACKTRACE_FRAMES"))
      {
        std::string value = _parsePrefixedValue(line, "STACKTRACE_FRAMES");
        if(!value.empty())
          config.m_stackTraceMaxFrames = _parseStackTraceFrames(value, logger_config_t::kMaxStackTraceFrames);
      }
      else if(_hasPrefix(line, "HINT"))
      {
        std::string value = _parsePrefixedValue(line, "HINT");
        if(!value.empty())
          config.m_createHintFile = _shouldShowStackTrace(value); // Используем ту же логику парсинга true/false
      }
      else if(_hasPrefix(line, "PRESET"))
      {
        std::string value = _parsePrefixedValue(line, "PRESET");
        if(!value.empty())
        {
          // Парсим список компонентов, разделенных запятыми
          config.m_presetComponents = _parsePresetComponents(value);
        }
      }
      else if(_hasPrefix(line, "BUFFERING"))
      {
        std::string value = _parsePrefixedValue(line, "BUFFERING");
        if(!value.empty())
          config.m_bufferingEnabled = _shouldShowStackTrace(value); // Используем ту же логику парсинга true/false
      }
      else if(_hasPrefix(line, "BUFFER_SIZE"))
      {
        std::string value = _parsePrefixedValue(line, "BUFFER_SIZE");
        if(!value.empty())
        {
          try
          {
            size_t bufferSize = std::stoull(value);
            // Устанавливаем минимальный размер 1
            config.m_bufferSize = (bufferSize > 0) ? bufferSize : logger_config_t::kBufferSize;
          }
          catch(...)
          {
            // В случае ошибки используем значение по умолчанию
            config.m_bufferSize = logger_config_t::kBufferSize;
          }
        }
      }
      // Поддержка старого формата для обратной совместимости
      else
      {
        // Если строка не содержит префиксов, парсим как старый формат
        // Для обратной совместимости поддерживаем старый формат без префиксов
        // В этом случае порядок строк все еще важен
        static int oldFormatLineNumber = 0;
        oldFormatLineNumber++;

        if(oldFormatLineNumber == 1)
        {
          // Первая строка без префикса - уровень логирования
          config.m_logLevel = _stringToLogLevel(line);
        }
        else if(oldFormatLineNumber == 2)
        {
          // Вторая строка без префикса - настройка имени функции
          config.m_funcNameMode = _parseFunctionNameMode(line);
        }
        else if(oldFormatLineNumber == 3)
        {
          // Третья строка без префикса - настройка timestamped logs
          config.m_useTimestampedLogs = _shouldUseTimestampedLogs(line);
        }
      }
    }

    return config;
  }
  catch(...)
  {
    // В случае любой ошибки используем значения по умолчанию
    return logger_config_t{};
  }
}

LogLevel
Logger::_stringToLogLevel(std::string const &levelStr) noexcept
{
  try
  {
    // Нормализуем строку (верхний регистр + удаление пробелов)
    std::string normalizedStr = _normalizeString(levelStr);

    // Сравниваем с известными уровнями
    if(normalizedStr == "TRACE") return LogLevel::LEVEL_TRACE;
    if(normalizedStr == "DEBUG") return LogLevel::LEVEL_DEBUG;
    if(normalizedStr == "INFO") return LogLevel::LEVEL_INFO;
    if(normalizedStr == "SUCCESS") return LogLevel::LEVEL_SUCCESS;
    if(normalizedStr == "WARNING") return LogLevel::LEVEL_WARNING;
    if(normalizedStr == "ERROR") return LogLevel::LEVEL_ERROR;
    if(normalizedStr == "FATAL") return LogLevel::LEVEL_FATAL;

    return LogLevel::LEVEL_INFO; // По умолчанию INFO
  }
  catch(...)
  {
    // В случае любой ошибки используем INFO по умолчанию
    return LogLevel::LEVEL_INFO;
  }
}

FunctionNameMode
Logger::_parseFunctionNameMode(std::string const &funcNameStr) noexcept
{
  try
  {
    // Нормализуем строку (верхний регистр + удаление пробелов)
    std::string normalizedStr = _normalizeString(funcNameStr);

    // Парсим различные варианты (case-insensitive)
    if(normalizedStr == "NONE" || normalizedStr == "NO_FUNCNAME") return FunctionNameMode::NONE;
    if(normalizedStr == "SHORT") return FunctionNameMode::SHORT;
    if(normalizedStr == "FULL") return FunctionNameMode::FULL;

    // По умолчанию возвращаем FULL для обратной совместимости
    return FunctionNameMode::FULL;
  }
  catch(...)
  {
    // В случае любой ошибки возвращаем FULL по умолчанию
    return FunctionNameMode::FULL;
  }
}

bool
Logger::_shouldShowFunctionName(std::string const &funcNameStr) noexcept
{
  return _parseFunctionNameMode(funcNameStr) != FunctionNameMode::NONE;
}

bool
Logger::_shouldShowStackTrace(std::string const &stackTraceStr) noexcept
{
  try
  {
    // Нормализуем строку (верхний регистр + удаление пробелов)
    std::string normalizedStr = _normalizeString(stackTraceStr);

    // Проверяем различные варианты "true"
    return (normalizedStr == "TRUE" || normalizedStr == "YES" || normalizedStr == "1");
  }
  catch(...)
  {
    // В случае любой ошибки возвращаем false
    return false;
  }
}

short
Logger::_parseStackTraceFrames(std::string const &framesStr, short defaultFrames) noexcept
{
  try
  {
    if(framesStr.empty()) return defaultFrames;

    // Пытаемся преобразовать строку в число
    int frames = std::stoi(framesStr);

    // Ограничиваем диапазон от 1 до максимального значения (64 фрейма)
    constexpr int kMaxStackTraceFrames = 64;
    if(frames < 1) return 1;
    if(frames > kMaxStackTraceFrames) return static_cast<short>(kMaxStackTraceFrames);

    return static_cast<short>(frames);
  }
  catch(std::exception const &exc)
  {
    // В случае любой ошибки возвращаем значение по умолчанию
    std::cerr << "Warning: Error during parse stacktrace frames: " << exc.what()
              << ". Returning default frames: " << defaultFrames << '\n';
    return defaultFrames;
  }
  catch(...)
  {
    std::cerr << "Warning: Unknown error during parse stacktrace frames. Returning default frames: " << defaultFrames
              << '\n';
    return defaultFrames;
  }
}

bool
Logger::_shouldUseTimestampedLogs(std::string const &timestampedStr) noexcept
{
  try
  {
    // Нормализуем строку (верхний регистр + удаление пробелов)
    std::string normalizedStr = _normalizeString(timestampedStr);

    // Проверяем различные варианты "true"
    return (normalizedStr == "TIMESTAMPED" || normalizedStr == "TRUE" || normalizedStr == "YES"
            || normalizedStr == "1");
  }
  catch(...)
  {
    // В случае любой ошибки не используем timestamped logs
    return false;
  }
}

bool
Logger::_checkLogSizeLimits() const noexcept
{
  try
  {
    if(!m_loggingEnabled || m_logFilePath.empty()) return true;

    // Получаем свободное место на диске
    int64_t freeSpace = getFreeDiskSpace(m_logFilePath);
    if(freeSpace < 0) return true; // Если не можем получить информацию, разрешаем запись

    // Определяем, работаем ли с файлом или папкой
    bool isDirectory = m_useTimestampedLogs;

    // Вычисляем максимально допустимый размер
    int64_t maxAllowedSize = _calculateMaxAllowedSize(freeSpace, isDirectory);

    // Получаем текущий размер
    int64_t currentSize = 0;
    if(isDirectory)
    {
      // Для папки получаем общий размер всех файлов
      std::string logDir = m_executableDirectory;
      if(!logDir.empty() && logDir[logDir.length() - 1] != LOGGER_FILE_SEPARATOR) logDir += LOGGER_FILE_SEPARATOR;
      logDir += kLogDirectoryName;

      currentSize = getDirectorySize(logDir);
    }
    else
    {
      // Для файла получаем размер файла
      currentSize = getFileSize(m_logFilePath);
    }

    if(currentSize < 0) return true; // Если не можем получить размер, разрешаем запись

    // Проверяем, не превышен ли лимит
    if(currentSize > maxAllowedSize)
    {
      // Пытаемся очистить старые логи
      std::string logPath
        = isDirectory ? (m_executableDirectory + LOGGER_FILE_SEPARATOR + kLogDirectoryName) : m_logFilePath;

      constexpr double kCleanupRatio = 0.8; // Оставляем 80% от лимита
      int64_t targetSize             = static_cast<int64_t>(static_cast<double>(maxAllowedSize) * kCleanupRatio);
      return _cleanupOldLogs(logPath, targetSize);
    }

    return true;
  }
  catch(...)
  {
    // В случае любой ошибки разрешаем запись
    return true;
  }
}

int64_t
Logger::_calculateMaxAllowedSize(int64_t freeSpace, bool isDirectory) noexcept
{
  try
  {
    // Базовые лимиты
    int64_t baseLimit = isDirectory ? kMaxLogDirectorySizeBytes : kMaxLogFileSizeBytes;

    // Вычисляем лимит на основе свободного места
    auto freeSpaceLimit = static_cast<int64_t>(static_cast<double>(freeSpace) * kMinFreeSpaceRatio);

    // Учитываем минимальное свободное место
    int64_t adjustedFreeSpace   = std::max(static_cast<int64_t>(0), freeSpace - kMinFreeSpaceBytes);

    auto adjustedFreeSpaceLimit = static_cast<int64_t>(static_cast<double>(adjustedFreeSpace) * kMinFreeSpaceRatio);

    // Выбираем минимальный из всех лимитов
    int64_t finalLimit = std::min(baseLimit, freeSpaceLimit);
    finalLimit         = std::min(finalLimit, adjustedFreeSpaceLimit);

    // Не позволяем лимиту быть меньше 10 МБ
    constexpr int64_t kMinLogSize = static_cast<int64_t>(10) * static_cast<int64_t>(1024) * static_cast<int64_t>(1024);
    finalLimit                    = std::max(finalLimit, kMinLogSize);

    return finalLimit;
  }
  catch(...)
  {
    // В случае ошибки возвращаем базовый лимит
    return isDirectory ? kMaxLogDirectorySizeBytes : kMaxLogFileSizeBytes;
  }
}

bool
Logger::_cleanupOldLogs(std::string const &logPath, int64_t targetSize) noexcept
{
  try
  {
    if(logPath.empty()) return false;

    // Проверяем, это файл или папка
    int64_t currentSize = getFileSize(logPath);
    if(currentSize >= 0)
    {
      // Это файл - просто очищаем его
      if(currentSize > targetSize)
      {
        std::ofstream file(logPath, std::ios::out | std::ios::trunc);
        return file.good();
      }
      return true;
    }

    // Это папка - получаем список файлов и удаляем старые
    std::vector<std::pair<std::string, std::time_t>> logFiles = _getLogFilesSortedByTime(logPath);

    int64_t totalSize                                         = getDirectorySize(logPath);
    if(totalSize < 0) return false;

    for(auto const &filePair : logFiles)
    {
      if(totalSize <= targetSize) break;

      int64_t fileSize = getFileSize(filePair.first);
      if(fileSize > 0)
      {
        // Удаляем файл
        if(std::remove(filePair.first.c_str()) == 0) totalSize -= fileSize;
      }
    }

    return totalSize <= targetSize;
  }
  catch(...)
  {
    return false;
  }
}

// NOLINTBEGIN(cppcoreguidelines-avoid-do-while, cppcoreguidelines-pro-bounds-array-to-pointer-decay)
std::vector<std::pair<std::string, std::time_t>>
Logger::_getLogFilesSortedByTime(std::string const &directoryPath) noexcept
{
  std::vector<std::pair<std::string, std::time_t>> result;

  try
  {
    if(directoryPath.empty()) return result;

#if defined(_WIN32) || defined(_WIN64)
    // Windows: используем FindFirstFile/FindNextFile
    std::string searchPath = directoryPath + "\\*";

    WIN32_FIND_DATAA findData{};
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);

    if(hFind != INVALID_HANDLE_VALUE)
    {
      do {
        if(strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0)
        {
          if((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0U)
          {
            std::string fullPath = directoryPath + "\\" + findData.cFileName;

            // Получаем время создания файла
            FILETIME creationTime = findData.ftCreationTime;
            ULARGE_INTEGER timeValue{};
            timeValue.LowPart  = creationTime.dwLowDateTime;
            timeValue.HighPart = creationTime.dwHighDateTime;

            // Константы для преобразования FILETIME в time_t
            constexpr uint64_t kFileTimeToSeconds = 10000000ULL;    // 100-nanosecond intervals to seconds
            constexpr uint64_t kEpochOffset       = 11644473600ULL; // Seconds between 1601-01-01 and 1970-01-01

            auto fileTime = static_cast<std::time_t>((timeValue.QuadPart / kFileTimeToSeconds) - kEpochOffset);
            result.emplace_back(fullPath, fileTime);
          }
        }
      } while(FindNextFileA(hFind, &findData) != 0);

      FindClose(hFind);
    }

#else
    // POSIX: используем opendir/readdir
    DIR *dir = opendir(directoryPath.c_str());
    if(dir != nullptr)
    {
      struct dirent *entry;
      while((entry = readdir(dir)) != nullptr)
      {
        if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0)
        {
          std::string fullPath = directoryPath + "/" + entry->d_name;

          struct stat fileStat{};
          if(stat(fullPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode))
            result.emplace_back(fullPath, fileStat.st_mtime);
        }
      }
      closedir(dir);
    }
#endif

    // Сортируем по времени создания (старые первые)
    std::sort(
      result.begin(), result.end(),
      [](std::pair<std::string, std::time_t> const &firstFile, std::pair<std::string, std::time_t> const &secondFile)
      { return firstFile.second < secondFile.second; });
  }
  catch(...)
  {
    // В случае ошибки возвращаем пустой список
    return {};
  }

  return result;
}
// NOLINTEND(cppcoreguidelines-avoid-do-while, cppcoreguidelines-pro-bounds-array-to-pointer-decay)

bool
Logger::_canWriteLog(size_t messageSize) const noexcept
{
  try
  {
    if(!m_loggingEnabled) return false;

    // Проверяем текущие лимиты
    if(!_checkLogSizeLimits()) return false;

    // Дополнительная проверка: если сообщение очень большое (> 1 МБ),
    // проверяем, есть ли достаточно места
    constexpr size_t kLargeMessageThreshold      = static_cast<size_t>(1024) * static_cast<size_t>(1024); // 1 МБ
    constexpr double kMaxMessageToFreeSpaceRatio = 0.1; // Не более 10% от свободного места

    if(messageSize > kLargeMessageThreshold)
    {
      int64_t freeSpace = getFreeDiskSpace(m_logFilePath);
      if(freeSpace >= 0
         && static_cast<int64_t>(messageSize)
              > static_cast<int64_t>(static_cast<double>(freeSpace) * kMaxMessageToFreeSpaceRatio))
      {
        return false;
      }
    }

    return true;
  }
  catch(...)
  {
    return false;
  }
}

std::string
Logger::_formatSize(int64_t sizeInBytes) noexcept
{
  try
  {
    constexpr int64_t kKB = static_cast<int64_t>(1024);
    constexpr int64_t kMB = kKB * static_cast<int64_t>(1024);
    constexpr int64_t kGB = kMB * static_cast<int64_t>(1024);
    constexpr int64_t kTB = kGB * static_cast<int64_t>(1024);

    std::stringstream stringStream;
    stringStream << std::fixed << std::setprecision(2);

    if(sizeInBytes >= kTB)
      stringStream << (static_cast<double>(sizeInBytes) / kTB) << " TB";
    else if(sizeInBytes >= kGB)
      stringStream << (static_cast<double>(sizeInBytes) / kGB) << " GB";
    else if(sizeInBytes >= kMB)
      stringStream << (static_cast<double>(sizeInBytes) / kMB) << " MB";
    else if(sizeInBytes >= kKB)
      stringStream << (static_cast<double>(sizeInBytes) / kKB) << " KB";
    else
      stringStream << sizeInBytes << " bytes";

    return stringStream.str();
  }
  catch(...)
  {
    return "Unknown size";
  }
}

void
Logger::_flushBuffer()
{
  // Lock already held by caller

  if(!m_logFile.is_open() || m_logBuffer.empty()) return;

  try
  {
    // Записываем все записи из буфера в файл
    for(auto const &entry : m_logBuffer)
    {
      // Используем сохраненное время из буфера для записи
      auto raw_time = std::chrono::system_clock::to_time_t(entry.m_timestamp);
      auto ms_count = std::chrono::duration_cast<std::chrono::milliseconds>(entry.m_timestamp.time_since_epoch())
                      % std::chrono::milliseconds::period::den;

      std::stringstream sstream;
      struct tm timeinfo{};
#if defined(_WIN32) || defined(_WIN64)
      localtime_s(std::addressof(timeinfo), std::addressof(raw_time));
#else
      localtime_r(std::addressof(raw_time), std::addressof(timeinfo));
#endif
      sstream << std::put_time(std::addressof(timeinfo), "%Y-%m-%d %H:%M:%S");
      sstream << '.' << std::setfill('0') << std::setw(3) << ms_count.count();

      m_logFile << "[" << sstream.str() << "] "
                << "[" << _levelToString(entry.m_level) << "] " << entry.m_message << '\n';
    }

    // Очищаем буфер после записи
    m_logBuffer.clear();

    // Принудительно сбрасываем буферы в файл
    m_logFile.flush();
  }
  catch(std::ios_base::failure const &e)
  {
    std::cerr << "Error writing buffer to log: " << e.what() << '\n';
    throw;
  }
}

std::string
Logger::_extractComponentName(std::string const &message) noexcept
{
  try
  {
    if(message.empty()) return "";

    // Ищем формат "[ComponentName]: message"
    // Ищем первую открывающую скобку
    size_t openBracket = message.find('[');
    if(openBracket == std::string::npos) return "";

    // Ищем закрывающую скобку после открывающей
    size_t closeBracket = message.find(']', openBracket + 1);
    if(closeBracket == std::string::npos) return "";

    // Проверяем, что после закрывающей скобки идет двоеточие и пробел
    if(closeBracket + 2 >= message.length()) return "";
    if(message[closeBracket + 1] != ':' || message[closeBracket + 2] != ' ') return "";

    // Извлекаем имя компонента между скобками
    size_t componentLength = closeBracket - openBracket - 1;
    if(componentLength == 0) return "";

    return message.substr(openBracket + 1, componentLength);
  }
  catch(...)
  {
    return "";
  }
}

bool
Logger::_shouldLogByPreset(std::string const &message) const noexcept
{
  try
  {
    // Если пресет отключен, логируем все
    if(!m_presetEnabled) return true;

    // Если пресет пуст, не логируем ничего
    if(m_presetComponents.empty()) return false;

    // Извлекаем имя компонента из сообщения
    std::string componentName = _extractComponentName(message);

    // Если компонент не найден в сообщении, не логируем (т.к. пресет включен)
    if(componentName.empty()) return false;

    // Проверяем, есть ли компонент в пресете
    return m_presetComponents.find(componentName) != m_presetComponents.end();
  }
  catch(...)
  {
    // В случае ошибки логируем (безопасный вариант)
    return true;
  }
}
