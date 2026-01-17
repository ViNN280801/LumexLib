#ifndef LOGGER_LOGGER_HPP
#define LOGGER_LOGGER_HPP

#include <chrono>
#include <cstdint>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

// ============= OS Detection Macros =============
// Cross-platform OS macros for detecting operating systems
// Compatible with MSVC, GCC, and Clang compilers

// Reset all OS macros first
#undef LOGGER_OS_WINDOWS
#undef LOGGER_OS_LINUX
#undef LOGGER_OS_MAC
#undef LOGGER_OS_UNIX
#undef LOGGER_OS_UNKNOWN

// Windows (32-bit and 64-bit)
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
  #define LOGGER_OS_WINDOWS 1
  #if defined(_WIN64) || defined(__WIN64__)
    #define LOGGER_OS_WINDOWS_64 1
  #else
    #define LOGGER_OS_WINDOWS_32 1
  #endif
// Apple Platforms
#elif defined(__APPLE__)
  #include <TargetConditionals.h>
  #if TARGET_OS_MAC
    #define LOGGER_OS_MAC 1
    #define LOGGER_OS_MACOS 1
  #endif
  #define LOGGER_OS_APPLE 1
// Linux
#elif defined(__linux__) || defined(linux) || defined(__linux)
  #define LOGGER_OS_LINUX 1
// Generic Unix
#elif defined(__unix__) || defined(__unix) || defined(unix)
  #define LOGGER_OS_UNIX 1
// Unknown OS
#else
  #define LOGGER_OS_UNKNOWN 1
#endif

// Architecture Helpers
#if defined(_M_X64) || defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64)
  #define LOGGER_ARCH_X64 1
  #define LOGGER_ARCH_64BIT 1
#elif defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(i386)
  #define LOGGER_ARCH_X86 1
  #define LOGGER_ARCH_32BIT 1
#elif defined(_M_ARM64) || defined(__aarch64__)
  #define LOGGER_ARCH_ARM64 1
  #define LOGGER_ARCH_64BIT 1
#elif defined(_M_ARM) || defined(__arm__) || defined(__arm)
  #define LOGGER_ARCH_ARM 1
  #define LOGGER_ARCH_32BIT 1
#endif

// Convenience macros for common checks
#define LOGGER_OS_IS_WINDOWS() (defined(LOGGER_OS_WINDOWS))
#define LOGGER_OS_IS_LINUX() (defined(LOGGER_OS_LINUX))
#define LOGGER_OS_IS_MACOS() (defined(LOGGER_OS_MAC) || defined(LOGGER_OS_MACOS))
#define LOGGER_OS_IS_APPLE() (defined(LOGGER_OS_APPLE))
#define LOGGER_OS_IS_UNIX() (defined(LOGGER_OS_UNIX) || defined(LOGGER_OS_LINUX) || defined(LOGGER_OS_APPLE))

// File separator and path constants
#ifdef LOGGER_OS_WINDOWS
  #define LOGGER_FILE_SEPARATOR '\\'
  #ifndef MAX_PATH
    #define MAX_PATH 260
  #endif
  #define LOGGER_MAX_PATH MAX_PATH
#else
  #define LOGGER_FILE_SEPARATOR '/'
  #ifndef PATH_MAX
    #define PATH_MAX 4096
  #endif
  #define LOGGER_MAX_PATH PATH_MAX
#endif

// ============= Attribute Macros =============

// [[nodiscard]] and [[nodiscard("reason")]]
#if __cplusplus < 201703L
  #if defined(__GNUC__) || defined(__clang__)
    #define LOGGER_ATTRIBUTE_NODISCARD(msg) __attribute__((warn_unused_result))
  #elif defined(_MSC_VER)
    #define LOGGER_ATTRIBUTE_NODISCARD(msg) _Check_return_
  #else
    #define LOGGER_ATTRIBUTE_NODISCARD(msg)
  #endif
#elif __cplusplus == 201703L
  #define LOGGER_ATTRIBUTE_NODISCARD(msg) [[nodiscard]]
#else // __cplusplus >= 202002UL
  #define LOGGER_ATTRIBUTE_NODISCARD(msg) [[nodiscard(msg)]]
#endif

// [[deprecated]] and [[deprecated("reason")]]
#if __cplusplus >= 201402L
  #define LOGGER_ATTRIBUTE_DEPRECATED [[deprecated]]
  #define LOGGER_ATTRIBUTE_DEPRECATED_MSG(msg) [[deprecated(msg)]]
#elif defined(__GNUC__) || defined(__clang__)
  #define LOGGER_ATTRIBUTE_DEPRECATED __attribute__((deprecated))
  #define LOGGER_ATTRIBUTE_DEPRECATED_MSG(msg) __attribute__((deprecated(msg)))
#elif defined(_MSC_VER)
  #define LOGGER_ATTRIBUTE_DEPRECATED __declspec(deprecated)
  #define LOGGER_ATTRIBUTE_DEPRECATED_MSG(msg) __declspec(deprecated(msg))
#else
  #define LOGGER_ATTRIBUTE_DEPRECATED
  #define LOGGER_ATTRIBUTE_DEPRECATED_MSG(msg)
#endif

// [[maybe_unused]]
#define LOGGER_ATTRIBUTE_MAYBE_UNUSED_VAR(var) (void)(var)
#if __cplusplus >= 201703L
  #define LOGGER_ATTRIBUTE_MAYBE_UNUSED [[maybe_unused]]
#elif defined(__GNUC__) || defined(__clang__)
  #define LOGGER_ATTRIBUTE_MAYBE_UNUSED __attribute__((unused))
#else
  #define LOGGER_ATTRIBUTE_MAYBE_UNUSED
#endif

// ============= Function Name Macros =============

#if defined(_WIN32) || defined(__WIN32__) || defined(__WIN64__) || defined(__MINGW32__) || defined(__MINGW64__)
  #define LOGGER_FUNCTION_NAME __FUNCSIG__
#elif defined(__GNUC__) || defined(__clang__)
  #define LOGGER_FUNCTION_NAME __PRETTY_FUNCTION__
#else
  #define LOGGER_FUNCTION_NAME __func__
#endif

// Short function name (just function name without signature) - standard __func__ (C99/C++11)
#define LOGGER_FUNCTION_NAME_SHORT __func__

// ============= Constexpr and Noexcept Macros =============

#if __cplusplus >= 201103L
  #define LOGGER_NOEXCEPT_FUNCTION noexcept
#else
  #define LOGGER_NOEXCEPT_FUNCTION
#endif

#if __cplusplus >= 202303L
  #define LOGGER_CONSTEXPR_FUNCTION constexpr
#elif __cplusplus >= 201103L
  #define LOGGER_CONSTEXPR_FUNCTION constexpr
#else
  #define LOGGER_CONSTEXPR_FUNCTION
#endif

#if __cplusplus >= 202002L
  #define LOGGER_CONSTINIT constinit
#elif __cplusplus >= 201103L
  #define LOGGER_CONSTINIT constexpr
#else
  #define LOGGER_CONSTINIT
#endif

#if __cplusplus >= 201703L
  #define LOGGER_INLINE_VARIABLE inline
#else
  #define LOGGER_INLINE_VARIABLE
#endif

#if defined(_MSC_VER)
  #define LOGGER_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
  #define LOGGER_RESTRICT __restrict__
#else
  #define LOGGER_RESTRICT
#endif

#define LOGGER_BASE_CONSTANT static LOGGER_INLINE_VARIABLE const
#define LOGGER_CONSTINIT_CONSTANT LOGGER_BASE_CONSTANT LOGGER_CONSTINIT
#define LOGGER_STRING_CONSTANT LOGGER_BASE_CONSTANT char* LOGGER_RESTRICT const

#define LOGGER_CONST LOGGER_BASE_CONSTANT
#define LOGGER_CONST_STR LOGGER_STRING_CONSTANT
#define LOGGER_CONST_NUM LOGGER_CONSTINIT_CONSTANT

#define LOGGER_ALIGNMENT_LOGGER_CONFIG 128
#define LOGGER_ALIGNMENT_LOG_ENTRY 64

// ============= C++11 Compatibility Layer =============

#if __cplusplus < 201402L // C++11 or earlier
  #include <memory>
  #include <utility>
namespace std
{
  template <typename T, typename... Args>
  typename std::enable_if<!std::is_array<T>::value, std::unique_ptr<T>>::type
  make_unique(Args &&...args)
  {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
  }

  template <typename T>
  typename std::enable_if<std::is_array<T>::value, std::unique_ptr<T>>::type
  make_unique(size_t n)
  {
    typedef typename std::remove_extent<T>::type U;
    return std::unique_ptr<T>(new U[n]);
  }

  template <class T, class U = T>
  LOGGER_CONSTEXPR_FUNCTION T
  exchange(T &obj, U &&new_value) noexcept(std::is_nothrow_move_constructible<T>::value
                                           && std::is_nothrow_assignable<T &, U>::value)
  {
    T old_value = std::move(obj);
    obj         = std::forward<U>(new_value);
    return old_value;
  }
} // namespace std
#endif

// ============= Stringify Function =============

// C++11/C++14 compatibility layer
#if __cplusplus < 201703L
template <typename... T> using void_t = void;
#else
using std::void_t;
#endif

// C++20 concepts-based approach
#if __cplusplus >= 202002L

template <typename T>
concept Streamable = requires(T &&type, std::ostream &ostream) { ostream << std::forward<T>(type); };

template <typename... Args>
concept AllStreamable = (Streamable<std::decay_t<Args>> && ...);

#else

// C++11/C++14/C++17 SFINAE-based approach
template <typename T, typename Enable = void> struct is_streamable : std::false_type {};

template <typename T>
struct is_streamable<T, void_t<decltype(std::declval<std::ostream &>() << std::declval<T>())>> : std::true_type {};

  #if __cplusplus >= 201402L
template <typename T> constexpr bool is_streamable_v = is_streamable<std::decay_t<T>>::value;
  #endif

  #if __cplusplus >= 201703L
template <typename... Args> constexpr bool all_streamable_v = (is_streamable<std::decay_t<Args>>::value && ...);
  #else
template <typename... Args> struct all_streamable;

template <typename First, typename... Rest> struct all_streamable<First, Rest...> {
  static constexpr bool value
    = is_streamable<typename std::decay<First>::type>::value && all_streamable<Rest...>::value;
};

template <> struct all_streamable<> {
  static constexpr bool value = true;
};

    #if __cplusplus >= 201402L
template <typename... Args> constexpr bool all_streamable_v = all_streamable<Args...>::value;
    #endif
  #endif

#endif // __cplusplus >= 202002L

// Stringify implementation variants
#if __cplusplus >= 202002L

// C++20+ concepts version
template <AllStreamable... Args>
inline std::string
stringify(Args &&...args)
{
  if constexpr(sizeof...(args) == 0) { return ""; }
  else
  {
    std::ostringstream oss;
    ((oss << std::forward<Args>(args)), ...);
    return oss.str();
  }
}

#elif __cplusplus >= 201703L

// C++17 version with fold expressions and if constexpr
template <typename... Args>
inline std::string
stringify(Args &&...args)
{
  static_assert(all_streamable_v<Args...>, "All arguments must be streamable");

  if constexpr(sizeof...(args) == 0) { return ""; }
  else
  {
    std::ostringstream oss;
    ((oss << std::forward<Args>(args)), ...);
    return oss.str();
  }
}

#elif __cplusplus >= 201402L

// C++14 version with variable templates
template <typename... Args>
inline std::string
stringify(Args &&...args)
{
  static_assert(all_streamable_v<Args...>, "All arguments must be streamable");

  if((sizeof...(args) == 0) ? true : false) return "";

  std::ostringstream oss;
  (void)std::initializer_list<int>{(oss << std::forward<Args>(args), 0)...};
  return oss.str();
}

#else

// C++11 version
template <typename... Args>
inline std::string
stringify(Args &&...args)
{
  static_assert(all_streamable<Args...>::value, "All arguments must be streamable");

  if((sizeof...(args) == 0) ? true : false) return "";

  std::ostringstream oss;
  (void)std::initializer_list<int>{(oss << std::forward<Args>(args), 0)...};
  return oss.str();
}

#endif

// Helper function for empty case optimization
inline std::string
stringify() noexcept
{
  return {};
}

/**
 * @brief Уровни логирования для классификации сообщений по важности.
 *
 * Уровни логирования позволяют фильтровать сообщения по их критичности.
 * От LEVEL_TRACE (самый детальный) до LEVEL_FATAL (критическая ошибка).
 */
enum class LogLevel : uint8_t
{
  LEVEL_TRACE   = 0, ///< Детальная отладочная информация
  LEVEL_DEBUG   = 1, ///< Отладочная информация
  LEVEL_INFO    = 2, ///< Общая информация
  LEVEL_SUCCESS = 3, ///< Успешное выполнение
  LEVEL_WARNING = 4, ///< Предупреждения
  LEVEL_ERROR   = 5, ///< Ошибки
  LEVEL_FATAL   = 6  ///< Критические ошибки
};

/**
 * @brief Режим отображения имени функции в логах.
 */
enum class FunctionNameMode : uint8_t
{
  NONE  = 0, ///< Не показывать имя функции
  SHORT = 1, ///< Показывать короткое имя функции (__func__)
  FULL  = 2  ///< Показывать полную сигнатуру функции (__FUNCSIG__/__PRETTY_FUNCTION__)
};

/**
 * @brief Конфигурация логгера, читаемая из файла enable_logs.
 *
 * Структура содержит все настройки логгера, которые могут быть заданы
 * через конфигурационный файл. Обеспечивает типобезопасность и расширяемость.
 */
struct alignas(LOGGER_ALIGNMENT_LOGGER_CONFIG) logger_config_t {
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  /// Сначала большие типы (8+ bytes)
  /// ~40 bytes
  std::unordered_set<std::string> m_presetComponents; ///< Набор компонентов для пресета (пусто = пресет отключен)
  /// 8 bytes
  LOGGER_CONST_NUM size_t kBufferSize = 100;         ///< Размер буфера логов (по умолчанию 100 записей)
  size_t m_bufferSize                 = kBufferSize; ///< Размер буфера логов (по умолчанию 100 записей)

  /// Затем средние типы (2-4 bytes)
  LOGGER_CONST_NUM short kMaxStackTraceFrames = 16; ///< Максимальное количество фреймов в stacktrace (по умолчанию 16)
  short m_stackTraceMaxFrames
    = kMaxStackTraceFrames; ///< Максимальное количество фреймов в stacktrace (по умолчанию 16)

  /// Затем маленькие типы (1 byte) - группируем вместе
  LogLevel m_logLevel             = LogLevel::LEVEL_INFO;   ///< Минимальный уровень логирования
  FunctionNameMode m_funcNameMode = FunctionNameMode::FULL; ///< Режим отображения имени функции
  bool m_useTimestampedLogs       = false;                  ///< Использовать ли timestamped логи
  bool m_showStackTrace           = false; ///< Показывать ли stacktrace (по умолчанию false, но true для WARNING/ERROR)
  bool m_createHintFile   = true;  ///< Создавать ли файл-хелпер с подсказкой для разработчиков (по умолчанию true)
  bool m_bufferingEnabled = false; ///< Включена ли буферизация логов (по умолчанию false)
  /// + 2 bytes padding для выравнивания до 8 bytes
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  /**
   * @brief Конструктор по умолчанию с настройками по умолчанию.
   */
  logger_config_t() = default;

  /**
   * @brief Конструктор с явными параметрами.
   * @param level Уровень логирования.
   * @param funcMode Режим отображения имени функции.
   * @param timestamped Использовать ли timestamped логи.
   * @param stackTrace Показывать ли stacktrace.
   * @param maxFrames Максимальное количество фреймов в stacktrace.
   * @param hintFile Создавать ли файл-хелпер с подсказкой.
   * @param preset Набор компонентов для пресета.
   */
  logger_config_t(LogLevel level, FunctionNameMode funcMode, bool timestamped, bool stackTrace = false,
                  short maxFrames = kMaxStackTraceFrames, bool hintFile = true,
                  std::unordered_set<std::string> const &preset = std::unordered_set<std::string>{}) noexcept
      : m_presetComponents(preset),
        m_stackTraceMaxFrames(maxFrames),
        m_logLevel(level),
        m_funcNameMode(funcMode),
        m_useTimestampedLogs(timestamped),
        m_showStackTrace(stackTrace),
        m_createHintFile(hintFile)
  {}
};

/**
 * @brief Структура для хранения буферизованной записи лога.
 *
 * Используется для временного хранения логов в буфере перед записью в файл.
 * Содержит всю необходимую информацию для последующей записи.
 */
struct alignas(LOGGER_ALIGNMENT_LOG_ENTRY) log_entry_t {
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes)
  /// Большие типы (8+ bytes)
  std::string m_message;                             ///< Сообщение лога
  std::chrono::system_clock::time_point m_timestamp; ///< Временная метка записи

  /// Маленькие типы (1 byte)
  LogLevel m_level; ///< Уровень логирования
  /// Padding: 7 bytes для выравнивания до 8 bytes
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  /**
   * @brief Конструктор с параметрами.
   * @param level Уровень логирования.
   * @param message Сообщение лога.
   * @param timestamp Временная метка (по умолчанию текущее время).
   */
  log_entry_t(LogLevel level, std::string message,
              std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now()) noexcept
      : m_message(std::move(message)), m_timestamp(timestamp), m_level(level)
  {}
};

static inline std::string
LogLevelToString(LogLevel level) noexcept
{
  switch(level)
  {
  case LogLevel::LEVEL_TRACE: return "LEVEL_TRACE";
  case LogLevel::LEVEL_DEBUG: return "LEVEL_DEBUG";
  case LogLevel::LEVEL_INFO: return "LEVEL_INFO";
  case LogLevel::LEVEL_SUCCESS: return "LEVEL_SUCCESS";
  case LogLevel::LEVEL_WARNING: return "LEVEL_WARNING";
  case LogLevel::LEVEL_ERROR: return "LEVEL_ERROR";
  case LogLevel::LEVEL_FATAL: return "LEVEL_FATAL";
  }
  return "UNKNOWN";
}

/**
 * @brief Синглтон-логгер для записи сообщений в файл рядом с исполняемым файлом.
 *
 * Этот класс обеспечивает централизованное логирование в файл, который располагается
 * рядом с исполняемым файлом (.exe, .dll, .so, .AppImage) или библиотекой.
 * Логгер автоматически определяет расположение исполняемого файла и создает
 * рядом с ним файл лога, перезаписывая его при каждом запуске программы.
 *
 * Особенности:
 * - Автоматическое определение пути к исполняемому файлу
 * - Поддержка различных платформ (Windows, Linux, macOS)
 * - Потокобезопасность через мьютексы
 * - Автоматическое форматирование времени и уровня логирования
 * - Возможность фильтрации по уровню логирования
 * - Условное включение логирования через файл-триггер
 * - Настройка уровня логирования через содержимое файла-триггера
 * - Поддержка timestamped логов в директории Logger_logs/
 *
 * Инварианты:
 * - Файл лога создается только при наличии файла `kLoggingEnableFileName`
 * - Все операции записи защищены мьютексом
 * - Уровень логирования не может быть меньше LEVEL_TRACE или больше LEVEL_FATAL
 * - Логирование отключено по умолчанию, если файл-триггер отсутствует
 * - Уровень логирования по умолчанию: LEVEL_INFO
 * - Поддерживаемые уровни в файле: trace, debug, info, warning, error, fatal (case-insensitive)
 * - Timestamped логи создаются в директории Logger_logs/ с именами logger_DD.MM.YYYY-hh:mm:ss.log
 *
 * Потокобезопасность:
 * - Все операции защищены внутренним мьютексом
 * - Конкурентные вызовы безопасны
 *
 * Исключения:
 * - Конструктор: может выбросить std::runtime_error при ошибке создания файла
 * - Методы логирования: могут выбросить std::ios_base::failure при ошибке записи
 * - Наблюдатели: не выбрасывают исключений
 */
class Logger final
{
public:
  // Запрещаем копирование и присваивание для синглтона
  Logger(Logger const &)            = delete;
  Logger &operator=(Logger const &) = delete;
  Logger(Logger &&)                 = delete;
  Logger &operator=(Logger &&)      = delete;

  /**
   * @brief Получает единственный экземпляр логгера (синглтон).
   * @return Ссылка на экземпляр логгера.
   * @note Потокобезопасно, создает экземпляр при первом вызове.
   * @throws std::runtime_error при ошибке создания файла лога.
   */
  static Logger &getInstance();

  /**
   * @brief Устанавливает минимальный уровень логирования.
   * @param level Новый минимальный уровень логирования.
   * @note Сообщения с уровнем ниже установленного не будут записываться в лог.
   */
  void setLogLevel(LogLevel level) noexcept;

  /**
   * @brief Получает текущий минимальный уровень логирования.
   * @return Текущий минимальный уровень логирования.
   * @note Не выбрасывает исключений.
   */
  LogLevel getLogLevel() const noexcept;

  /**
   * @brief Записывает сообщение уровня LEVEL_TRACE.
   * @param message Сообщение для записи в лог.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Записывается только если текущий уровень логирования <= LEVEL_TRACE.
   */
  template <typename... Args>
  void
  trace(Args &&...args)
  {
    log(LogLevel::LEVEL_TRACE, stringify(std::forward<Args>(args)...));
  }

  /**
   * @brief Записывает сообщение уровня LEVEL_DEBUG.
   * @param message Сообщение для записи в лог.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Записывается только если текущий уровень логирования <= LEVEL_DEBUG.
   */
  template <typename... Args>
  void
  debug(Args &&...args)
  {
    log(LogLevel::LEVEL_DEBUG, stringify(std::forward<Args>(args)...));
  }

  /**
   * @brief Записывает сообщение уровня LEVEL_INFO.
   * @param message Сообщение для записи в лог.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Записывается только если текущий уровень логирования <= LEVEL_INFO.
   */
  template <typename... Args>
  void
  info(Args &&...args)
  {
    log(LogLevel::LEVEL_INFO, stringify(std::forward<Args>(args)...));
  }

  /**
   * @brief Записывает сообщение уровня LEVEL_SUCCESS.
   * @param message Сообщение для записи в лог.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Записывается только если текущий уровень логирования <= LEVEL_SUCCESS.
   */
  template <typename... Args>
  void
  success(Args &&...args)
  {
    log(LogLevel::LEVEL_SUCCESS, stringify(std::forward<Args>(args)...));
  }

  /**
   * @brief Записывает сообщение уровня LEVEL_WARNING.
   * @param message Сообщение для записи в лог.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Записывается только если текущий уровень логирования <= LEVEL_WARNING.
   */
  template <typename... Args>
  void
  warning(Args &&...args)
  {
    log(LogLevel::LEVEL_WARNING, stringify(std::forward<Args>(args)...));
  }

  /**
   * @brief Записывает сообщение уровня LEVEL_ERROR.
   * @param message Сообщение для записи в лог.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Записывается только если текущий уровень логирования <= LEVEL_ERROR.
   */
  template <typename... Args>
  void
  error(Args &&...args)
  {
    log(LogLevel::LEVEL_ERROR, stringify(std::forward<Args>(args)...));
  }

  /**
   * @brief Записывает сообщение уровня LEVEL_FATAL.
   * @param message Сообщение для записи в лог.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Записывается только если текущий уровень логирования <= LEVEL_FATAL.
   */
  template <typename... Args>
  void
  fatal(Args &&...args)
  {
    log(LogLevel::LEVEL_FATAL, stringify(std::forward<Args>(args)...));
  }

  /**
   * @brief Записывает сообщение с указанным уровнем логирования.
   * @param level Уровень логирования сообщения.
   * @param message Сообщение для записи в лог.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Записывается только если текущий уровень логирования <= level.
   */
  void log(LogLevel level, std::string const &message);

  /**
   * @brief Проверяет, включен ли указанный уровень логирования.
   * @param level Уровень логирования для проверки.
   * @return true, если уровень логирования включен, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  bool isLevelEnabled(LogLevel level) const noexcept;

  /**
   * @brief Проверяет, включено ли логирование в целом.
   * @details Если есть файл с названием `kLoggingEnableFileName` рядом с исполняемым файлом (или библиотекой), то
   * логирование будет включено. Если в файле указан уровень логирования (trace, debug, info, warning, error, fatal),
   * то будет использован этот уровень, иначе по умолчанию info. Поддерживается case-insensitive парсинг.
   * @return true, если логирование включено, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  bool isLoggingEnabled() const noexcept;

  /**
   * @brief Получает режим отображения имени функции в логах.
   * @return Режим отображения имени функции.
   * @note Не выбрасывает исключений.
   */
  FunctionNameMode getFunctionNameMode() const noexcept;

  /**
   * @brief Проверяет, нужно ли показывать имя функции в логах.
   * @return true, если нужно показывать имя функции, false в противном случае.
   * @note Не выбрасывает исключений.
   * @deprecated Используйте getFunctionNameMode() вместо этого метода.
   */
  LOGGER_ATTRIBUTE_DEPRECATED_MSG("Use getFunctionNameMode() instead, this method kept for backward compatibility")
  bool shouldShowFunctionName() const noexcept;

  /**
   * @brief Получает путь к файлу лога.
   * @return Путь к файлу лога в виде строки.
   * @note Не выбрасывает исключений.
   */
  std::string getLogFilePath() const noexcept;

  /**
   * @brief Выводит подсказку разработчикам о том, как использовать логгер.
   * @details Выводит информацию в stdout о создании файла-триггера и примерах конфигурации.
   *          Bilingual output: Russian and English.
   *          Полезно вызывать, когда логирование не работает из-за отсутствия файла-триггера
   *          или некорректной конфигурации.
   * @note Не выбрасывает исключений.
   */
  void printLoggerHint() const noexcept;

  /**
   * @brief Принудительно сбрасывает буферы в файл.
   * @throws std::ios_base::failure при ошибке сброса буферов.
   * @note Полезно для критических сообщений, которые должны быть записаны немедленно.
   */
  void flush();

  /**
   * @brief Включает буферизацию логов.
   * @param bufferSize Размер буфера (количество записей для хранения). По умолчанию logger_config_t::kBufferSize (100).
   * @note При включенной буферизации логи не записываются сразу, а сохраняются в буфере.
   *       При возникновении WARNING/ERROR/FATAL все записи из буфера записываются в файл.
   * @note Потокобезопасно.
   */
  void enableBuffering(size_t bufferSize = logger_config_t::kBufferSize) noexcept;

  /**
   * @brief Отключает буферизацию логов.
   * @note При отключении буферизации все логи записываются немедленно.
   *       Если в буфере есть записи, они будут записаны перед отключением.
   * @throws std::ios_base::failure при ошибке записи буфера.
   * @note Потокобезопасно.
   */
  void disableBuffering();

  /**
   * @brief Устанавливает размер буфера логов.
   * @param bufferSize Новый размер буфера (количество записей).
   * @note Если новый размер меньше текущего размера буфера, старые записи удаляются.
   * @note Потокобезопасно.
   */
  void setBufferSize(size_t bufferSize) noexcept;

  /**
   * @brief Проверяет, включена ли буферизация.
   * @return true, если буферизация включена, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  bool isBufferingEnabled() const noexcept;

  /**
   * @brief Получает текущий размер буфера.
   * @return Текущий размер буфера (количество записей).
   * @note Не выбрасывает исключений.
   */
  size_t getBufferSize() const noexcept;

  /**
   * @brief Включает фильтрацию по пресетам компонентов.
   * @param components Набор компонентов для фильтрации (логируются только эти компоненты).
   * @note Если пресет включен, логируются только сообщения от указанных компонентов.
   *       Формат компонента в сообщении: "[ComponentName]: message"
   * @note Потокобезопасно.
   */
  void enablePreset(std::unordered_set<std::string> const &components) noexcept;

  /**
   * @brief Отключает фильтрацию по пресетам.
   * @note После отключения логируются все сообщения независимо от компонента.
   * @note Потокобезопасно.
   */
  void disablePreset() noexcept;

  /**
   * @brief Устанавливает набор компонентов для пресета.
   * @param components Набор компонентов для фильтрации.
   * @note Автоматически включает пресет, если он был отключен.
   * @note Потокобезопасно.
   */
  void setPresetComponents(std::unordered_set<std::string> const &components) noexcept;

  /**
   * @brief Проверяет, включен ли пресет фильтрации.
   * @return true, если пресет включен, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  bool isPresetEnabled() const noexcept;

  /**
   * @brief Получает текущий набор компонентов пресета.
   * @return Набор компонентов пресета (копия).
   * @note Не выбрасывает исключений.
   */
  std::unordered_set<std::string> getPresetComponents() const noexcept;

  /**
   * @brief Устанавливает имя файла-триггера для включения логирования.
   * @param fileName Имя файла-триггера (например, "enable_logs").
   * @note Файл-триггер должен находиться рядом с исполняемым файлом.
   * @note Изменение имени файла-триггера не перечитывает конфигурацию автоматически.
   *       Для применения изменений может потребоваться перезапуск программы.
   * @note Потокобезопасно.
   */
  void setTriggerFileName(std::string const &fileName) noexcept;

  /**
   * @brief Получает текущее имя файла-триггера.
   * @return Имя файла-триггера.
   * @note Не выбрасывает исключений.
   */
  std::string getTriggerFileName() const noexcept;

private:
  /**
   * @brief Приватный конструктор для синглтона.
   * @throws std::runtime_error при ошибке создания файла лога.
   */
  Logger();

  /**
   * @brief Деструктор, закрывает файл лога.
   */
  ~Logger();

  /**
   * @brief Определяет путь к исполняемому файлу.
   * @return Путь к директории, содержащей исполняемый файл.
   * @throws std::runtime_error при ошибке определения пути.
   */
  std::string _getExecutableDirectory() const;

  /**
   * @brief Создает полный путь к файлу лога.
   * @return Полный путь к файлу лога.
   */
  std::string _createLogFilePath() const;

  /**
   * @brief Создает путь к обычному файлу лога (перезаписываемый).
   * @return Полный путь к обычному файлу лога.
   */
  std::string _createSingleLogFilePath() const;

  /**
   * @brief Создает путь к timestamped файлу лога в директории Logger_logs/.
   * @return Полный путь к timestamped файлу лога.
   */
  std::string _createTimestampedLogFilePath() const;

  /**
   * @brief Форматирует текущее время для имени файла в формате DD.MM.YYYY-hh:mm:ss.
   * @return Отформатированная строка времени для имени файла.
   */
  static std::string _formatTimestampForFilename();

  /**
   * @brief Форматирует текущее время для записи в лог.
   * @return Отформатированная строка времени.
   */
  static std::string _formatTimestamp();

  /**
   * @brief Преобразует уровень логирования в строку.
   * @param level Уровень логирования.
   * @return Строковое представление уровня логирования.
   */
  static std::string _levelToString(LogLevel level);

  /**
   * @brief Записывает сообщение в лог с проверкой уровня.
   * @param level Уровень логирования сообщения.
   * @param message Сообщение для записи.
   * @throws std::ios_base::failure при ошибке записи в файл.
   */
  void _writeLog(LogLevel level, std::string const &message);

  /**
   * @brief Извлекает директорию из полного пути к файлу.
   * @param fullPath Полный путь к файлу.
   * @return Путь к директории, содержащей файл.
   * @note Кроссплатформенная реализация без использования std::filesystem.
   */
  static std::string _extractDirectoryFromPath(std::string const &fullPath);

  /**
   * @brief Проверяет существование файла включения логирования и читает уровень логирования.
   * @return true, если файл-триггер (по умолчанию enable_logs) существует, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  bool _isLoggingEnabledFileExists() const noexcept;

  /**
   * @brief Читает конфигурацию из файла включения логирования.
   * @return Структура logger_config_t с настройками логгера.
   * @note Не выбрасывает исключений.
   */
  logger_config_t _readConfigFromFile() const noexcept;

  /**
   * @brief Преобразует строку в уровень логирования.
   * @param levelStr Строка с уровнем логирования (case-insensitive).
   * @return Соответствующий LogLevel или LEVEL_INFO по умолчанию.
   * @note Не выбрасывает исключений.
   */
  static LogLevel _stringToLogLevel(std::string const &levelStr) noexcept;

  /**
   * @brief Парсит строку в режим отображения имени функции.
   * @param funcNameStr Строка с настройкой (case-insensitive): "none"/"NO_FUNCNAME", "short", "signature".
   * @return Режим отображения имени функции.
   * @note Не выбрасывает исключений.
   */
  static FunctionNameMode _parseFunctionNameMode(std::string const &funcNameStr) noexcept;

  /**
   * @brief Проверяет, нужно ли показывать имя функции.
   * @param funcNameStr Строка с настройкой (case-insensitive).
   * @return true, если нужно показывать имя функции, false если NO_FUNCNAME.
   * @note Не выбрасывает исключений.
   * @deprecated Используйте _parseFunctionNameMode() вместо этого метода.
   */
  LOGGER_ATTRIBUTE_DEPRECATED_MSG("Use _parseFunctionNameMode() instead, this method kept for backward compatibility")
  static bool _shouldShowFunctionName(std::string const &funcNameStr) noexcept;

  /**
   * @brief Парсит строку в значение включения stacktrace.
   * @param stackTraceStr Строка с настройкой (case-insensitive): "true"/"yes"/"1" или "false"/"no"/"0".
   * @return true, если нужно показывать stacktrace, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  static bool _shouldShowStackTrace(std::string const &stackTraceStr) noexcept;

  /**
   * @brief Парсит строку в количество фреймов для stacktrace.
   * @param framesStr Строка с числом фреймов.
   * @param defaultFrames Значение по умолчанию, если парсинг не удался.
   * @return Количество фреймов для stacktrace.
   * @note Не выбрасывает исключений.
   */
  static short _parseStackTraceFrames(std::string const &framesStr, short defaultFrames) noexcept;

  /**
   * @brief Проверяет, нужно ли использовать timestamped logs.
   * @param timestampedStr Строка с настройкой (case-insensitive).
   * @return true, если нужно использовать timestamped logs, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  static bool _shouldUseTimestampedLogs(std::string const &timestampedStr) noexcept;

  /**
   * @brief Проверяет, не превышает ли размер логов допустимые лимиты.
   * @details Проверяет размер файла или папки логов относительно свободного места на диске.
   *          Использует динамические лимиты на основе доступного места.
   * @return true, если размер в пределах нормы, false если превышен лимит.
   * @note Не выбрасывает исключений.
   */
  bool _checkLogSizeLimits() const noexcept;

  /**
   * @brief Вычисляет максимально допустимый размер логов на основе свободного места.
   * @details Учитывает как абсолютные лимиты, так и процент от свободного места.
   * @param freeSpace Свободное место на диске в байтах.
   * @param isDirectory true для папки логов, false для файла лога.
   * @return Максимально допустимый размер в байтах.
   * @note Не выбрасывает исключений.
   */
  static int64_t _calculateMaxAllowedSize(int64_t freeSpace, bool isDirectory) noexcept;

  /**
   * @brief Очищает старые логи при превышении лимитов.
   * @details Удаляет самые старые файлы логов для освобождения места.
   * @param logPath Путь к файлу или папке логов.
   * @param targetSize Целевой размер после очистки в байтах.
   * @return true, если очистка выполнена успешно, false в противном случае.
   * @note Не выбрасывает исключений.
   */
  static bool _cleanupOldLogs(std::string const &logPath, int64_t targetSize) noexcept;

  /**
   * @brief Получает список файлов логов, отсортированный по времени создания.
   * @param directoryPath Путь к директории с логами.
   * @return Вектор пар (путь_к_файлу, время_создания), отсортированный по времени (старые первые).
   * @note Не выбрасывает исключений.
   */
  static std::vector<std::pair<std::string, std::time_t>>
  _getLogFilesSortedByTime(std::string const &directoryPath) noexcept;

  /**
   * @brief Проверяет, можно ли записать лог без превышения лимитов.
   * @details Выполняет полную проверку размера перед записью.
   * @param messageSize Размер сообщения, которое планируется записать.
   * @return true, если запись разрешена, false если превышен лимит.
   * @note Не выбрасывает исключений.
   */
  bool _canWriteLog(size_t messageSize) const noexcept;

  /**
   * @brief Форматирует размер в байтах в читаемый вид.
   * @param sizeInBytes Размер в байтах.
   * @return Строка с отформатированным размером (например, "1.5 GB").
   * @note Не выбрасывает исключений.
   */
  static std::string _formatSize(int64_t sizeInBytes) noexcept;

  /**
   * @brief Записывает все записи из буфера в файл.
   * @details Используется при возникновении WARNING/ERROR/FATAL для записи истории логов.
   * @throws std::ios_base::failure при ошибке записи в файл.
   * @note Вызывается только при заблокированном мьютексе.
   */
  void _flushBuffer();

  /**
   * @brief Извлекает имя компонента из сообщения.
   * @param message Сообщение лога в формате "[ComponentName]: message" или просто "message".
   * @return Имя компонента, если найдено, иначе пустая строка.
   * @note Не выбрасывает исключений.
   */
  static std::string _extractComponentName(std::string const &message) noexcept;

  /**
   * @brief Проверяет, должен ли лог быть записан согласно пресету.
   * @param message Сообщение лога.
   * @return true, если лог должен быть записан, false в противном случае.
   * @note Если пресет отключен, всегда возвращает true.
   * @note Не выбрасывает исключений.
   */
  bool _shouldLogByPreset(std::string const &message) const noexcept;

  /**
   * @brief Парсит строку со списком компонентов для пресета.
   * @param componentsStr Строка с компонентами, разделенными запятыми (например, "A,B,E").
   * @return Набор компонентов для пресета.
   * @note Не выбрасывает исключений.
   */
  static std::unordered_set<std::string> _parsePresetComponents(std::string const &componentsStr) noexcept;

  std::ofstream m_logFile;             ///< Поток для записи в файл лога
  std::string m_logFilePath;           ///< Путь к файлу лога
  LogLevel m_currentLogLevel;          ///< Текущий минимальный уровень логирования
  mutable std::mutex m_logMutex;       ///< Мьютекс для защиты операций записи
  std::string m_executableDirectory;   ///< Директория исполняемого файла
  bool m_loggingEnabled;               ///< Флаг включения логирования
  FunctionNameMode m_functionNameMode; ///< Режим отображения имени функции в логах
  bool m_useTimestampedLogs;           ///< Флаг использования timestamped логов
  bool m_showStackTrace;               ///< Флаг показа stacktrace в логах
  short m_stackTraceMaxFrames;         ///< Максимальное количество фреймов в stacktrace
  mutable bool m_hintShown;            ///< Флаг того, что подсказка уже была показана
  std::string m_triggerFileName;       ///< Имя файла-триггера для включения логирования

  // Буферизация логов
  bool m_bufferingEnabled;             ///< Флаг включения буферизации
  size_t m_bufferSize;                 ///< Размер буфера (количество записей)
  std::deque<log_entry_t> m_logBuffer; ///< Буфер для хранения логов

  // Пресеты для фильтрации по компонентам
  bool m_presetEnabled;                               ///< Флаг включения пресета
  std::unordered_set<std::string> m_presetComponents; ///< Набор компонентов для фильтрации

  std::chrono::system_clock::time_point m_logFileStartTime; ///< Время начала лога
  std::chrono::system_clock::time_point m_logFileEndTime;   ///< Время окончания лога

  LOGGER_CONST_STR kDefaultLogFileName     = "logger.log";
  LOGGER_CONST_STR kLogDirectoryName       = "Logger_logs";
  LOGGER_CONST_STR kDefaultTriggerFileName = "enable_logs";
  LOGGER_CONST_STR kHintFileName           = "LOGGER_README.txt";

  // Константы для контроля размера логов
  LOGGER_CONST_NUM int64_t kMaxLogFileSizeBytes
    = static_cast<int64_t>(1024) * static_cast<int64_t>(1024)
      * static_cast<int64_t>(1024); ///< Максимальный размер файла лога (1 ГБ)
  LOGGER_CONST_NUM int64_t kMaxLogDirectorySizeBytes
    = static_cast<int64_t>(2) * static_cast<int64_t>(1024) * static_cast<int64_t>(1024)
      * static_cast<int64_t>(1024);                  ///< Максимальный размер папки логов (2 ГБ)
  LOGGER_CONST_NUM double kMinFreeSpaceRatio  = 0.1; ///< Минимальная доля свободного места (10%)
  LOGGER_CONST_NUM int64_t kMinFreeSpaceBytes = static_cast<int64_t>(100) * static_cast<int64_t>(1024)
                                                * static_cast<int64_t>(1024); ///< Минимальное свободное место (100 МБ)
};

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

/**
 * @brief Макросы для удобного логирования с автоматическим определением функции.
 *
 * @details Эти макросы предоставляют удобный способ логирования с автоматическим
 *          добавлением имени функции и форматированием сообщений. Макросы проверяют
 *          уровень логирования на этапе компиляции и не выполняют никаких операций,
 *          если уровень отключен.
 *
 *          Макросы используют LOGGER_FUNCTION_NAME для получения имени текущей функции
 *          в зависимости от компилятора и платформы.
 *
 *          Макросы поддерживают variadic arguments и автоматически конвертируют их
 *          в строку с помощью функции stringify().
 *
 * @note Все макросы автоматически добавляют имя функции к сообщению в формате:
 *       "[FunctionName] message"
 *
 * @example Примеры использования:
 * @code
 * void myFunction() {
 *   LOGGER_LOG_TRACE("Entering function");
 *   LOGGER_LOG_DEBUG("Processing data:", value, "bytes");
 *   LOGGER_LOG_INFO("Operation completed successfully");
 *   LOGGER_LOG_SUCCESS("Operation completed successfully");
 *   LOGGER_LOG_WARNING("Deprecated API used, consider using:", newApiName);
 *   LOGGER_LOG_ERROR("Failed to open file:", filename, "error code:", errorCode);
 *   LOGGER_LOG_FATAL("Critical system error, shutting down");
 * }
 * @endcode
 */

#define LOGGER_LOG_IF(level, ...)                                                                                        \
  if (::Logger::getInstance().isLevelEnabled(level))                                                                       \
  {                                                                                                                        \
    ::FunctionNameMode const funcMode = ::Logger::getInstance().getFunctionNameMode();                                     \
    if(funcMode == ::FunctionNameMode::NONE)                                                                               \
    {                                                                                                                      \
      ::Logger::getInstance().log(level, stringify(__VA_ARGS__));                                                          \
    }                                                                                                                      \
    else if(funcMode == ::FunctionNameMode::SHORT)                                                                         \
    {                                                                                                                      \
      ::Logger::getInstance().log(level, std::string("[") + LOGGER_FUNCTION_NAME_SHORT + "] " + stringify(__VA_ARGS__)); \
    }                                                                                                                      \
    else                                                                                                                   \
    {                                                                                                                      \
      ::Logger::getInstance().log(level, std::string("[") + LOGGER_FUNCTION_NAME + "] " + stringify(__VA_ARGS__));       \
    }                                                                                                                      \
  }                                                                                                                        \
  else                                                                                                                     \
    (void)0

#define LOGGER_LOG_TRACE(...)   LOGGER_LOG_IF(::LogLevel::LEVEL_TRACE,   __VA_ARGS__)
#define LOGGER_LOG_DEBUG(...)   LOGGER_LOG_IF(::LogLevel::LEVEL_DEBUG,   __VA_ARGS__)
#define LOGGER_LOG_INFO(...)    LOGGER_LOG_IF(::LogLevel::LEVEL_INFO,    __VA_ARGS__)
#define LOGGER_LOG_SUCCESS(...) LOGGER_LOG_IF(::LogLevel::LEVEL_SUCCESS, __VA_ARGS__)
#define LOGGER_LOG_WARNING(...) LOGGER_LOG_IF(::LogLevel::LEVEL_WARNING, __VA_ARGS__)
#define LOGGER_LOG_ERROR(...)   LOGGER_LOG_IF(::LogLevel::LEVEL_ERROR,   __VA_ARGS__)
#define LOGGER_LOG_FATAL(...)   LOGGER_LOG_IF(::LogLevel::LEVEL_FATAL,   __VA_ARGS__)

#define LOGGER_ADDRESS_TO_STRING(address) ("<0x" + std::to_string(reinterpret_cast<uintptr_t>(address)) + ">")

#define LOGGER_LOG_ADDRESS_TRACE(message, address)   LOGGER_LOG_IF(::LogLevel::LEVEL_TRACE,   (message) + LOGGER_ADDRESS_TO_STRING(address))
#define LOGGER_LOG_ADDRESS_DEBUG(message, address)   LOGGER_LOG_IF(::LogLevel::LEVEL_DEBUG,   (message) + LOGGER_ADDRESS_TO_STRING(address))
#define LOGGER_LOG_ADDRESS_INFO(message, address)    LOGGER_LOG_IF(::LogLevel::LEVEL_INFO,    (message) + LOGGER_ADDRESS_TO_STRING(address))
#define LOGGER_LOG_ADDRESS_SUCCESS(message, address) LOGGER_LOG_IF(::LogLevel::LEVEL_SUCCESS, (message) + LOGGER_ADDRESS_TO_STRING(address))
#define LOGGER_LOG_ADDRESS_WARNING(message, address) LOGGER_LOG_IF(::LogLevel::LEVEL_WARNING, (message) + LOGGER_ADDRESS_TO_STRING(address))
#define LOGGER_LOG_ADDRESS_ERROR(message, address)   LOGGER_LOG_IF(::LogLevel::LEVEL_ERROR,   (message) + LOGGER_ADDRESS_TO_STRING(address))
#define LOGGER_LOG_ADDRESS_FATAL(message, address)   LOGGER_LOG_IF(::LogLevel::LEVEL_FATAL,   (message) + LOGGER_ADDRESS_TO_STRING(address))

#define LOGGER_LOG_OBJECT_TRACE(object, ...)   LOGGER_LOG_IF(::LogLevel::LEVEL_TRACE,   "Object " + LOGGER_ADDRESS_TO_STRING(object) + ": " + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_DEBUG(object, ...)   LOGGER_LOG_IF(::LogLevel::LEVEL_DEBUG,   "Object " + LOGGER_ADDRESS_TO_STRING(object) + ": " + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_INFO(object, ...)    LOGGER_LOG_IF(::LogLevel::LEVEL_INFO,    "Object " + LOGGER_ADDRESS_TO_STRING(object) + ": " + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_SUCCESS(object, ...) LOGGER_LOG_IF(::LogLevel::LEVEL_SUCCESS, "Object " + LOGGER_ADDRESS_TO_STRING(object) + ": " + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_WARNING(object, ...) LOGGER_LOG_IF(::LogLevel::LEVEL_WARNING, "Object " + LOGGER_ADDRESS_TO_STRING(object) + ": " + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_ERROR(object, ...)   LOGGER_LOG_IF(::LogLevel::LEVEL_ERROR,   "Object " + LOGGER_ADDRESS_TO_STRING(object) + ": " + __VA_ARGS__)
#define LOGGER_LOG_OBJECT_FATAL(object, ...)   LOGGER_LOG_IF(::LogLevel::LEVEL_FATAL,   "Object " + LOGGER_ADDRESS_TO_STRING(object) + ": " + __VA_ARGS__)

// --------------------------------------------------------------------------------------- //
// Макросы с коротким названием для удобства использования.                                //
// --------------------------------------------------------------------------------------- //
#define LOG_TRACE(...)   LOGGER_LOG_TRACE(__VA_ARGS__)
#define LOG_DEBUG(...)   LOGGER_LOG_DEBUG(__VA_ARGS__)
#define LOG_INFO(...)    LOGGER_LOG_INFO(__VA_ARGS__)
#define LOG_SUCCESS(...) LOGGER_LOG_SUCCESS(__VA_ARGS__)
#define LOG_WARNING(...) LOGGER_LOG_WARNING(__VA_ARGS__)
#define LOG_ERROR(...)   LOGGER_LOG_ERROR(__VA_ARGS__)
#define LOG_FATAL(...)   LOGGER_LOG_FATAL(__VA_ARGS__)

#define LOG_ADDRESS_TRACE(message, address)   LOGGER_LOG_ADDRESS_TRACE(message, address)
#define LOG_ADDRESS_DEBUG(message, address)   LOGGER_LOG_ADDRESS_DEBUG(message, address)
#define LOG_ADDRESS_INFO(message, address)    LOGGER_LOG_ADDRESS_INFO(message, address)
#define LOG_ADDRESS_SUCCESS(message, address) LOGGER_LOG_ADDRESS_SUCCESS(message, address)
#define LOG_ADDRESS_WARNING(message, address) LOGGER_LOG_ADDRESS_WARNING(message, address)
#define LOG_ADDRESS_ERROR(message, address)   LOGGER_LOG_ADDRESS_ERROR(message, address)
#define LOG_ADDRESS_FATAL(message, address)   LOGGER_LOG_ADDRESS_FATAL(message, address)

#define LOG_OBJECT_TRACE(object, ...)   LOGGER_LOG_OBJECT_TRACE(object, __VA_ARGS__)
#define LOG_OBJECT_DEBUG(object, ...)   LOGGER_LOG_OBJECT_DEBUG(object, __VA_ARGS__)
#define LOG_OBJECT_INFO(object, ...)    LOGGER_LOG_OBJECT_INFO(object, __VA_ARGS__)
#define LOG_OBJECT_SUCCESS(object, ...) LOGGER_LOG_OBJECT_SUCCESS(object, __VA_ARGS__)
#define LOG_OBJECT_WARNING(object, ...) LOGGER_LOG_OBJECT_WARNING(object, __VA_ARGS__)
#define LOG_OBJECT_ERROR(object, ...)   LOGGER_LOG_OBJECT_ERROR(object, __VA_ARGS__)
#define LOG_OBJECT_FATAL(object, ...)   LOGGER_LOG_OBJECT_FATAL(object, __VA_ARGS__)
// ---------------------------------------------------------------------------------------

// NOLINTEND(cppcoreguidelines-macro-usage)

#endif // !LOGGER_LOGGER_HPP
