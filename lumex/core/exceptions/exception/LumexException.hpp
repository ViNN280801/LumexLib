#ifndef LUMEX_EXCEPTION_HPP
#define LUMEX_EXCEPTION_HPP

#include "lumex/LumexExport.hpp"

#include "lumex/core/exceptions/stacktrace/LumexStacktrace.hpp"

// ================================================================== //
// ====================== Lumex Base Exception ====================== //
// ================================================================== //

#if __cplusplus >= 201703L
  #include <string_view>
#endif

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace Core
  {
    namespace Exceptions
    {
// Suppress C4275 warning for std::exception base class not having DLL interface
#ifdef _WIN32
  #pragma warning(push)
  #pragma warning(disable : 4275 4251)
#endif
      class LUMEX_API LumexBaseException : public std::exception
      {
      public:
        /**
         * @brief Constructs a `LumexBaseException` with a message.
         * @param message The error message.
         */
        LumexBaseException(char const *message);

        /**
         * @brief Constructs a `LumexBaseException` with a message.
         * @param message The error message.
         */
        LumexBaseException(std::string const &message);

        /**
         * @brief Constructs a `LumexBaseException` with a message.
         * @param message The error message.
         */
        LumexBaseException(std::string &&message);

#if __cplusplus >= 201703L
        /**
         * @brief Constructs a `LumexBaseException` with a message.
         * @param message The error message.
         */
        LumexBaseException(std::string_view message);
#endif

        /**
         * @brief Returns the error message as a C-string.
         * @details This method overrides the `std::exception::what` method to return
         *          the error message as a C-string.
         * @return A pointer to the error message as a C-string.
         * @note This method is `noexcept` because it only returns a pointer to a member variable.
         */
        char const *
        what() const noexcept override
        {
          return m_message.c_str();
        }

        /**
         * @brief Returns the stack trace of the error.
         * @details This method returns the stack trace of the error.
         * @return The stack trace of the error.
         * @note This method is `noexcept` because it only returns a member variable.
         */
        LumexStacktrace
        getStackTrace() const noexcept
        {
          return m_stacktrace;
        }

        /**
         * @brief Write an error to the standard error stream by the following format:
         *        [exception_name] -> custom message
         *        Uses demangled exception name to avoid names like "NSt6vectorIiSaIiEEE" -> "std::vector<int,
         * std::allocator<int>>"
         * @example
         * [LumexException] -> Failed to open file
         */
        void to_stderr() const noexcept;

        /**
         * @brief Write a crash report to a file by pattern: "crash_report_{timestamp}.txt".
         * @param stacktrace The stack trace of the error.
         */
        void to_crash_report() const;

      private:
        std::string m_message;        ///< The custom error message to be displayed.
        LumexStacktrace m_stacktrace; ///< The stack trace of the error.
      };
#ifdef _WIN32
  #pragma warning(pop)
#endif
    } // namespace Exceptions
  } // namespace Core
} // namespace Lumex

// Declare the trampoline function
LUMEX_PUBLIC_API LUMEX_ATTRIBUTE_NOINLINE LumexStacktrace LumexException_GetStackTraceTrampoline(int skip_frames);

using LumexBaseException = Lumex::Core::Exceptions::LumexBaseException;

// ================================================================== //
// ====================== Lumex Exception Macro ===================== //
// ================================================================== //

// 1 option. Define the exception class.
#define LUMEX_DEFINE_EXCEPTION(exception_name, inherit_from)                        \
    class exception_name : public inherit_from                                      \
    {                                                                               \
    public:                                                                         \
        exception_name(char const *message) : inherit_from(message) {}              \
        exception_name(std::string const &message) : inherit_from(message) {}       \
        exception_name(std::string &&message) : inherit_from(std::move(message)) {} \
    };

// 2 option. Throw the exception.
// Pattern:
// [exception_name] -> custom message
// Example:
// [LumexException] -> Failed to open file
#include "lumex/core/string/LumexString"
#define LUMEX_THROW_EXCEPTION(exception_name, msg) \
    throw exception_name(stringify(lumDemangle(exception_name), ": ", msg));

// 3. Handle the exception.
#define LUMEX_EXCEPTION_HANDLE_BEGIN \
    try                              \
    {                                \
        SET_SEH_TRANSLATOR

#define LUMEX_EXCEPTION_HANDLE_END                                                \
    }                                                                             \
    catch (LumexBaseException const &ex)                                          \
    {                                                                             \
        ex.to_stderr();                                                           \
        ex.to_crash_report();                                                     \
    }                                                                             \
    catch (std::exception const &ex)                                              \
    {                                                                             \
        std::cerr << "[std::exception] " << ex.what() << '\n';                    \
        LumexBaseException(ex.what()).to_stderr();                                \
        LumexBaseException(ex.what()).to_crash_report();                          \
    }                                                                             \
    catch (...)                                                                   \
    {                                                                             \
        std::cerr << "[Unknown exception]\n";                                     \
    }

// ================================================================== //
// ====================== >>>>>>>>>>> <<<<<<<<< ===================== //
// ================================================================== //

#endif // !LUMEX_EXCEPTION_HPP
