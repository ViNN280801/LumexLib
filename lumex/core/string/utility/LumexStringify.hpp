#ifndef LUMEX_UTILITIES_HPP
#define LUMEX_UTILITIES_HPP

#include <sstream>
#include <string>
#include <type_traits>

namespace Lumex // NOLINT(modernize-concat-nested-namespaces)
{
  namespace String
  {
    namespace Utility
    {
      // ---------------------------------------------------------------------
      // C++11/C++14 compatibility layer
      // ---------------------------------------------------------------------

#if __cplusplus < 201703L
      // C++14 doesn't have std::void_t, C++17+ has it
      template <typename... T> using void_t = void; // NOLINT(readability-identifier-naming)
#else
      using std::void_t;
#endif

      // ---------------------------------------------------------------------
      // C++20 concepts-based approach
      // ---------------------------------------------------------------------

#if __cplusplus >= 202002L

      template <typename T>
      concept Streamable = requires(T &&type, std::ostream &ostream) { ostream << std::forward<T>(type); };

      template <typename... Args>
      concept AllStreamable = (Streamable<std::decay_t<Args>> && ...);

#else

      // ---------------------------------------------------------------------
      // C++11/C++14/C++17 SFINAE-based approach
      // ---------------------------------------------------------------------

      template <typename T, typename Enable = void> struct is_streamable : std::false_type {};

      template <typename T>
      struct is_streamable<T, void_t<decltype(std::declval<std::ostream &>() << std::declval<T>())>> : std::true_type {
      };

  #if __cplusplus >= 201402L
      // C++14+ variable templates
      template <typename T> constexpr bool is_streamable_v = is_streamable<std::decay_t<T>>::value;
  #endif

  #if __cplusplus >= 201703L
      // C++17+ fold expressions
      template <typename... Args> constexpr bool all_streamable_v = (is_streamable<std::decay_t<Args>>::value && ...);
  #else
      // C++11/C++14 recursive template approach
      template <typename... Args> struct all_streamable;

      template <typename First, typename... Rest> struct all_streamable<First, Rest...> {
        static constexpr bool value = is_streamable<std::decay_t<First>>::value && all_streamable<Rest...>::value;
      };

      template <> struct all_streamable<> {
        static constexpr bool value = true;
      };

    #if __cplusplus >= 201402L
      template <typename... Args> constexpr bool all_streamable_v = all_streamable<Args...>::value;
    #endif
  #endif

#endif // __cplusplus >= 202002L

      // ---------------------------------------------------------------------
      // Stringify implementation variants
      // ---------------------------------------------------------------------

#if __cplusplus >= 202002L

      // C++20+ concepts version
      template <AllStreamable... Args>
      std::string
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
      std::string
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
      std::string
      stringify(Args &&...args)
      {
        static_assert(all_streamable_v<Args...>, "All arguments must be streamable");

        if(sizeof...(args) == 0) return "";

        std::ostringstream oss;
        (void)std::initializer_list<int>{(oss << std::forward<Args>(args), 0)...};
        return oss.str();
      }

#else

      // C++11 version
      template <typename... Args>
      std::string
      stringify(Args &&...args)
      {
        static_assert(all_streamable<Args...>::value, "All arguments must be streamable");

        if(sizeof...(args) == 0) return "";

        std::ostringstream oss;
        (void)std::initializer_list<int>{(oss << std::forward<Args>(args), 0)...};
        return oss.str();
      }

#endif

      // ---------------------------------------------------------------------
      // Helper function for empty case optimization
      // ---------------------------------------------------------------------

      inline std::string
      stringify() noexcept
      {
        return {};
      }

      // ---------------------------------------------------------------------
      // C++20+ requires clause version (alternative implementation)
      // ---------------------------------------------------------------------

#if __cplusplus >= 202002L

      template <typename... Args>
        requires AllStreamable<Args...>
      std::string
      stringify_v2(Args &&...args)
      {
        if constexpr(sizeof...(args) == 0) { return {}; }
        else
        {
          std::ostringstream oss;
          ((oss << std::forward<Args>(args)), ...);
          return oss.str();
        }
      }

#endif

    } // namespace Utility
  } // namespace String
} // namespace Lumex

using Lumex::String::Utility::stringify;

#endif // !LUMEX_UTILITIES_HPP
